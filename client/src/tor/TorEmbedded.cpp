/**
 * TorEmbedded.cpp — Tor 子进程内嵌实现
 *
 * 搜索 tor.exe (优先级):
 *   1. ./tor/tor.exe       (Tor Expert Bundle)
 *   2. ./tor.exe           (当前目录)
 *   3. PATH 中的 tor.exe
 *
 * 启动时自动生成最小 torrc 配置。
 */
#include "tor/TorEmbedded.h"

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#define popen _popen
#define pclose _pclose
#define PATH_SEP "\\"
#else
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#define PATH_SEP "/"
#endif

#include <cstdio>
#include <cstring>
#include <sstream>
#include <fstream>

namespace chrono { namespace client { namespace tor {

// ============================================================
// 工具函数
// ============================================================

static std::string find_tor_exe()
{
    const char* candidates[] = {
        "tor\\tor.exe",
        "tor.exe",
        "build\\tor\\tor.exe",
    };
    for (auto* path : candidates) {
        FILE* f = fopen(path, "rb");
        if (f) { fclose(f); return path; }
    }
    return "tor.exe";  // let PATH search on Windows
}

static bool ensure_data_dir(const std::string& dir)
{
#ifdef _WIN32
    CreateDirectoryA(dir.c_str(), nullptr);
#else
    mkdir(dir.c_str(), 0700);
#endif
    return true;
}

// 生成最小 torrc 配置 (启用网桥选项)
static bool write_torrc(const std::string& data_dir)
{
    std::string torrc_path = data_dir + PATH_SEP + "torrc";
    std::ofstream f(torrc_path);
    if (!f) return false;

    f << "# Chrono-shift Tor 配置\n"
      << "SOCKSPort 9050\n"
      << "ControlPort 9051\n"
      << "CookieAuthentication 0\n"
      << "Log notice file " << data_dir << PATH_SEP << "tor.log\n"
      << "ClientOnly 1\n"
      << "DataDirectory " << data_dir << "\n"
      << "\n"
      << "# 国内网络环境建议启用网桥:\n"
      << "# UseBridges 1\n"
      << "# Bridge obfs4 <IP>:<PORT> <FINGERPRINT>\n"
      << "# ClientTransportPlugin obfs4 exec obfs4proxy.exe\n";
    return true;
}

// ============================================================
// TorEmbedded 实现
// ============================================================

TorEmbedded& TorEmbedded::instance()
{
    static TorEmbedded inst;
    return inst;
}

TorEmbedded::~TorEmbedded() { stop(); }

bool TorEmbedded::start(const std::string& tor_path_hint,
                         const std::string& data_dir)
{
    if (state_ == TorState::Starting ||
        state_ == TorState::Bootstrapping ||
        state_ == TorState::Running)
        return true;

    state_ = TorState::Starting;
    log_buf_.clear();
    bootstrap_pct_ = 0;
    circuits_ = 0;

    // 查找 tor.exe
    std::string tor_exe = tor_path_hint.empty()
        ? find_tor_exe() : tor_path_hint;

    // 确保数据目录存在
    std::string effective_data = data_dir.empty() ? "tor_data" : data_dir;
    ensure_data_dir(effective_data);
    write_torrc(effective_data);

    // 构建命令行 (SOCKS5:9050, Control:9051, 无密码认证)
    std::string cmd = "\"" + tor_exe + "\""
        " --SOCKSPort 9050"
        " --ControlPort 9051"
        " --CookieAuthentication 0"
        " --DataDirectory \"" + effective_data + "\""
        " --Log \"notice stdout\""
        " --ClientOnly 1"
        " 2>&1";

    log_buf_ += "[*] 启动: " + tor_exe + "\n";
    log_buf_ += "[*] 数据目录: " + effective_data + "\n";

    // 后台线程运行 Tor
    worker_ = std::thread([this, cmd]() {
        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) {
            log_buf_ += "[-] 无法启动 Tor 子进程\n";
            state_ = TorState::Failed;
            if (state_cb_) state_cb_(TorState::Failed);
            return;
        }

        state_ = TorState::Bootstrapping;
        if (state_cb_) state_cb_(TorState::Bootstrapping);

        char buf[1024];
        while (fgets(buf, sizeof(buf), pipe)) {
            {
                std::lock_guard<std::mutex> lock(log_mutex_);
                log_buf_ += buf;
                if (log_buf_.size() > 65536)
                    log_buf_ = log_buf_.substr(log_buf_.size() - 32768);
            }

            // 解析 bootstrap 进度: "Bootstrapped 45%: ..."
            const char* p = strstr(buf, "Bootstrapped ");
            if (p) {
                int pct = 0;
                sscanf(p, "Bootstrapped %d%%", &pct);
                if (pct > bootstrap_pct_) bootstrap_pct_ = pct;
                if (pct >= 100 && state_ != TorState::Running) {
                    state_ = TorState::Running;
                    if (state_cb_) state_cb_(TorState::Running);
                }
            }

            // 检测错误
            if (strstr(buf, "[err]") || strstr(buf, "[warn]")) {
                // 记录但不中断
            }
        }

        int ret = pclose(pipe);
        (void)ret;
        if (state_ != TorState::Stopped && state_ != TorState::Running) {
            state_ = TorState::Failed;
            if (state_cb_) state_cb_(TorState::Failed);
        }
    });

    return true;
}

void TorEmbedded::stop()
{
    TorState expected = state_.load();
    if (expected == TorState::Stopped) return;
    state_ = TorState::Stopped;

    // 尝试通过 ControlPort 发送 QUIT
    // (Tor 收到 QUIT 后优雅退出, popen 的 pipe 会关闭)
#ifdef _WIN32
    // 简单方式: 终止子进程
    // 更好的方式是发送 SIGNAL SHUTDOWN 到 ControlPort
#endif

    if (worker_.joinable()) {
        worker_.detach();  // 分离线程让进程自然退出
    }
}

std::string TorEmbedded::get_log() const
{
    std::lock_guard<std::mutex> lock(log_mutex_);
    size_t max_len = 4000;
    if (log_buf_.size() > max_len)
        return "...(省略 " + std::to_string(log_buf_.size() - max_len)
               + " 字节)...\n" + log_buf_.substr(log_buf_.size() - max_len);
    return log_buf_;
}

} } } // namespace
