/**
 * I2pdEmbedded.cpp — i2pd 子进程内嵌实现
 *
 * 搜索 i2pd.exe (优先级):
 *   1. ./i2pd/i2pd.exe     (下载的二进制)
 *   2. ./i2pd.exe           (当前目录)
 *   3. PATH 中的 i2pd.exe
 *
 * i2pd 通过 SAM API (localhost:7656) 提供服务。
 */
#include "i2p/I2pdEmbedded.h"

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#define popen _popen
#define pclose _pclose
#else
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>
#define popen popen
#define pclose pclose
#endif

#include <cstdio>
#include <cstring>
#include <sstream>
#include <fstream>
#include <chrono>

namespace chrono { namespace client { namespace i2p {

// ============================================================
// 工具函数
// ============================================================

static std::string find_i2pd_exe()
{
    const char* candidates[] = {
        "i2pd/i2pd.exe",
        "i2pd.exe",
        "build/i2pd/i2pd.exe",
        "i2pd",
    };
    for (auto* path : candidates) {
        FILE* f = fopen(path, "rb");
        if (f) { fclose(f); return path; }
    }
#ifdef _WIN32
    return "i2pd.exe";
#else
    return "i2pd";
#endif
}

static bool ensure_dir(const std::string& dir)
{
#ifdef _WIN32
    CreateDirectoryA(dir.c_str(), nullptr);
    return true;
#else
    mkdir(dir.c_str(), 0700);
    return true;
#endif
}

// 生成 i2pd 配置文件
static bool write_i2pd_conf(const std::string& data_dir, uint16_t sam_port)
{
    std::string conf_path = data_dir + "/i2pd.conf";
    std::ofstream f(conf_path);
    if (!f) return false;

    f << "# Chrono-shift i2pd 配置\n"
      << "log = stdout\n"
      << "loglevel = info\n"
      << "datadir = " << data_dir << "\n"
      << "\n"
      << "sam.enabled = true\n"
      << "sam.address = 127.0.0.1\n"
      << "sam.port = " << sam_port << "\n"
      << "\n"
      << "httpproxy.enabled = false\n"
      << "socksproxy.enabled = false\n"
      << "bob.enabled = false\n"
      << "i2pcontrol.enabled = false\n"
      << "upnp.enabled = false\n"
      << "nat = false\n"
      << "ipv6 = false\n"
      << "reseed.verify = true\n"
      << "reseed.threshold = 25\n";
    return true;
}

// ============================================================
// I2pdEmbedded 实现
// ============================================================

I2pdEmbedded& I2pdEmbedded::instance()
{
    static I2pdEmbedded inst;
    return inst;
}

I2pdEmbedded::~I2pdEmbedded() { stop(); }

bool I2pdEmbedded::start(const std::string& data_dir, uint16_t sam_port)
{
    if (state_ == I2pdState::Starting ||
        state_ == I2pdState::Integrating ||
        state_ == I2pdState::Running)
        return true;

    data_dir_ = data_dir.empty() ? "./i2p_data" : data_dir;
    sam_port_ = sam_port;
    state_ = I2pdState::Starting;
    log_buf_.clear();
    start_time_ = std::chrono::steady_clock::now();
    nodes_ = 0;
    tunnels_ = 0;

    // 查找 i2pd.exe
    std::string i2pd_exe = find_i2pd_exe();

    // 确保数据目录和配置
    ensure_dir(data_dir_);
    write_i2pd_conf(data_dir_, sam_port_);

    // 构建命令行
    std::string cmd = "\"" + i2pd_exe + "\""
        " --datadir=\"" + data_dir_ + "\""
        " --conf=\"" + data_dir_ + "/i2pd.conf\""
        " 2>&1";

    log_buf_ += "[*] 启动: " + i2pd_exe + "\n";
    log_buf_ += "[*] 数据目录: " + data_dir_ + "\n";
    log_buf_ += "[*] SAM 端口: " + std::to_string(sam_port_) + "\n";

    // 后台线程运行 i2pd
    worker_ = std::thread([this, cmd]() {
        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) {
            std::lock_guard<std::mutex> lock(log_mutex_);
            log_buf_ += "[-] 无法启动 i2pd\n"
                        "    请从 https://github.com/PurpleI2P/i2pd/releases 下载\n";
            state_ = I2pdState::Failed;
            if (state_cb_) state_cb_(I2pdState::Failed);
            return;
        }

        state_ = I2pdState::Reseeding;
        if (state_cb_) state_cb_(I2pdState::Reseeding);

        char buf[1024];
        while (fgets(buf, sizeof(buf), pipe)) {
            {
                std::lock_guard<std::mutex> lock(log_mutex_);
                log_buf_ += buf;
                if (log_buf_.size() > 65536)
                    log_buf_ = log_buf_.substr(log_buf_.size() - 32768);
            }

            // 解析 NetDb 状态
            if (strstr(buf, "NetDb") && strstr(buf, "success")) {
                state_ = I2pdState::Integrating;
                if (state_cb_) state_cb_(I2pdState::Integrating);
            }

            // 解析节点数: "Known: 1500"
            const char* p = strstr(buf, "Known:");
            if (p) sscanf(p, "Known: %d", &nodes_);

            // 解析隧道: "Tunnels active: 5"
            p = strstr(buf, "Tunnels");
            if (p && strstr(buf, "active"))
                sscanf(buf, "Tunnels active: %d", &tunnels_);

            // 检测就绪: SAM bridge started
            if (strstr(buf, "SAM") && strstr(buf, "start")) {
                state_ = I2pdState::Running;
                if (state_cb_) state_cb_(I2pdState::Running);
            }

            // 检测错误
            if (strstr(buf, "[error]") || strstr(buf, "Error")) {
                // 记录但继续运行
            }
        }

        pclose(pipe);
        if (state_ != I2pdState::Stopped) {
            state_ = I2pdState::Failed;
            if (state_cb_) state_cb_(I2pdState::Failed);
        }
    });

    return true;
}

void I2pdEmbedded::stop()
{
    I2pdState expected = state_.load();
    if (expected == I2pdState::Stopped) return;
    state_ = I2pdState::Stopped;

    if (worker_.joinable()) worker_.detach();
}

std::string I2pdEmbedded::get_log() const
{
    std::lock_guard<std::mutex> lock(log_mutex_);
    size_t max_len = 4000;
    if (log_buf_.size() > max_len)
        return "...(省略 " + std::to_string(log_buf_.size() - max_len)
               + " 字节)...\n" + log_buf_.substr(log_buf_.size() - max_len);
    return log_buf_;
}

std::string I2pdEmbedded::get_integration_status() const
{
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        now - start_time_).count();

    std::ostringstream oss;
    switch (state_) {
    case I2pdState::Stopped:     oss << "已停止"; break;
    case I2pdState::Starting:    oss << "启动中"; break;
    case I2pdState::Reseeding:   oss << "下载网络数据库 (reseed)"; break;
    case I2pdState::Integrating: oss << "集成到 I2P 网络"; break;
    case I2pdState::Running:     oss << "运行中"; break;
    case I2pdState::Failed:      oss << "启动失败"; break;
    }
    oss << " | 运行 " << elapsed << "s"
        << " | 节点 " << nodes_
        << " | 隧道 " << tunnels_;
    return oss.str();
}

int I2pdEmbedded::uptime_seconds() const
{
    if (state_ != I2pdState::Running) return 0;
    auto now = std::chrono::steady_clock::now();
    return (int)std::chrono::duration_cast<std::chrono::seconds>(
        now - start_time_).count();
}

} } } // namespace
