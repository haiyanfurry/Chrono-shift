/**
 * cmd_tor.cpp — Tor 网络管理命令 (默认传输层)
 *
 * 支持三种模式:
 *   嵌入模式: TorEmbedded (源码编译 tor.exe → 子进程管理)
 *   外部模式: Tor SOCKS5:9050 + ControlPort:9051 (需用户安装 Tor)
 *   自动模式: 优先嵌入, 不可用时回退外部
 *
 * I2P 为备选传输层 (见 cmd_i2p.cpp)
 */
#include "../devtools_cli.hpp"
#include "print_compat.h"
#include <string>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <thread>
#include <chrono>

#include "tor/TorClient.h"
#include "tor/TorEmbedded.h"
#include "social/SocialManager.h"

namespace cli = chrono::client::cli;
using namespace chrono::client::tor;
using namespace chrono::client::social;

static TorClient g_tor_client;

// 辅助: 格式化字节数
static std::string format_bytes(uint64_t bytes)
{
    if (bytes < 1024) return std::to_string(bytes) + " B";
    if (bytes < 1048576) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1) << (bytes / 1024.0) << " KB";
        return oss.str();
    }
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << (bytes / 1048576.0) << " MB";
    return oss.str();
}

// ============================================================
// tor 命令 (默认传输层)
// ============================================================
static int cmd_tor(int argc, char** argv)
{
    auto& mgr = SocialManager::instance();

    if (argc < 1) {
        cli::println("Tor — 默认传输层");
        cli::println("");
        cli::println("  tor start [mode]   - 启动 Tor (嵌入/外部/自动)");
        cli::println("  tor status         - 查看连接状态、电路、流量");
        cli::println("  tor log            - 查看 Tor 启动日志");
        cli::println("  tor circuits       - 查看活跃电路");
        cli::println("  tor newid          - 请求新身份 (NEWNYM)");
        cli::println("  tor stop           - 停止 Tor");
        cli::println("  tor onion          - 显示 Onion 地址");
        cli::println("");
        cli::println("  模式: embedded (嵌入子进程) | external (外部代理)");
        return -1;
    }

    const char* sub = argv[0];

    // ---- tor start ----
    if (std::strcmp(sub, "start") == 0) {
        const char* mode = (argc >= 2) ? argv[1] : "auto";

        // 嵌入模式: 启动内嵌 Tor 子进程
        auto try_embedded = [&]() -> bool {
            cli::println("[*] 启动内嵌 Tor (子进程)...");
            auto& emb = chrono::client::tor::TorEmbedded::instance();
            emb.start("", "tor_data");
            cli::println("[*] 等待 Tor 就绪 (SOCKS5:9050)...");

            // 检测 SOCKS5 端口是否可连 (最多等15秒)
            bool ready = false;
            for (int i = 0; i < 30; i++) {
                if (g_tor_client.connect_socks("127.0.0.1", 9050)) {
                    ready = true;
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
            if (ready) {
                cli::println("{}[+] Tor 已就绪 — SOCKS5:9050{}", cli::COLOR_GREEN, cli::COLOR_RESET);
                g_tor_client.connect_control("127.0.0.1", 9051);
                return true;
            }
            cli::println("{}[!] Tor 启动超时 (15s){}", cli::COLOR_YELLOW, cli::COLOR_RESET);
            cli::println("    可能原因: 国内需配置网桥 (bridge) 才能连接 Tor 网络");
            cli::println("    查看日志: tor log");
            return false;
        };

        if (std::strcmp(mode, "embedded") == 0) {
            return try_embedded() ? 0 : -1;
        }

        if (std::strcmp(mode, "external") == 0 || std::strcmp(mode, "auto") == 0) {
            if (std::strcmp(mode, "auto") == 0) {
                // 自动模式: 优先尝试嵌入
                if (try_embedded()) return 0;
                cli::println("[*] 回退到外部 SOCKS5 模式...");
            }

            // 外部模式: SOCKS5 + ControlPort
            cli::println("[*] 正在连接外部 Tor 代理 (127.0.0.1:9050)...");
            bool socks_ok = g_tor_client.connect_socks("127.0.0.1", 9050);
            if (socks_ok) {
                cli::println("{}[+] Tor SOCKS5 代理已连接{}", cli::COLOR_GREEN, cli::COLOR_RESET);
                g_tor_client.connect_control("127.0.0.1", 9051);
                auto status = g_tor_client.get_status();
                if (!status.version.empty())
                    cli::println("    版本: {}", status.version);
                if (!status.fingerprint.empty())
                    cli::println("    指纹: {}", status.fingerprint.substr(0, 12) + "...");
                cli::println("{}[+] Tor 就绪 — 默认传输层已激活{}",
                    cli::COLOR_GREEN, cli::COLOR_RESET);
                return 0;
            }

            cli::println("{}[!] Tor SOCKS5 连接失败: {}{}",
                cli::COLOR_YELLOW, g_tor_client.last_error(), cli::COLOR_RESET);
            cli::println("    原因: Tor 守护进程未运行或端口 9050 未开放");
            cli::println("");
            cli::println("{}  Tor 连接失败 — 可能的原因:{}", cli::COLOR_YELLOW, cli::COLOR_RESET);
            cli::println("    1. Tor 未安装 → https://www.torproject.org/download/");
            cli::println("    2. 网络限制   → 国内需配置网桥 (bridge)");
            cli::println("    3. 防火墙阻止 → 检查 9050/9051 端口");
            cli::println("");
            cli::println("  {}备选方案: 使用 I2P 传输层{}", cli::COLOR_CYAN, cli::COLOR_RESET);
            cli::println("    > i2p start");
            return -1;
        }

        cli::println(stderr, "未知模式: {} (可用: embedded, external, auto)", mode);
        return -1;
    }

    // ---- tor status ----
    if (std::strcmp(sub, "status") == 0) {
        auto& emb = chrono::client::tor::TorEmbedded::instance();

        cli::println("  Tor 传输层状态:");
        if (emb.is_ready() || emb.state() == TorState::Bootstrapping ||
            emb.state() == TorState::Starting) {
            cli::println("    模式:       {}内嵌子进程{}",
                emb.is_ready() ? "" : "{}", "");
            if (!emb.is_ready())
                cli::println("    模式:       {}启动中 ({}%){}",
                    cli::COLOR_YELLOW, emb.bootstrap_progress(), cli::COLOR_RESET);
            else
                cli::println("    模式:       {}内嵌子进程 (运行中){}",
                    cli::COLOR_GREEN, cli::COLOR_RESET);
            cli::println("    Bootstrap:  {}%", emb.bootstrap_progress());
            cli::println("    电路数:     {}", emb.circuit_count());
        } else {
            cli::println("    SOCKS5 代理: {}",
                g_tor_client.is_socks_ready() ? "已连接 :9050" : "未连接");
            cli::println("    ControlPort: {}",
                g_tor_client.is_control_ready() ? "已连接 :9051" : "未连接");

            if (g_tor_client.is_control_ready()) {
                auto s = g_tor_client.get_status();
                cli::println("    版本:        {}", s.version);
                cli::println("    读取流量:    {}", format_bytes(s.bytes_read));
                cli::println("    写入流量:    {}", format_bytes(s.bytes_written));
            }
        }

        cli::println("    社交 UID:    {}", mgr.my_uid().empty() ? "(未设置)" : mgr.my_uid());
        if (!mgr.my_i2p().empty())
            cli::println("    Onion 地址:  {}", mgr.my_i2p());
        cli::println("    好友数量:    {}", mgr.friend_list().size());

        auto pending = mgr.pending_requests();
        if (!pending.empty())
            cli::println("    {}[!] {} 个待处理好友请求{}",
                cli::COLOR_YELLOW, pending.size(), cli::COLOR_RESET);
        return 0;
    }

    // ---- tor log ----
    if (std::strcmp(sub, "log") == 0) {
        auto& emb = chrono::client::tor::TorEmbedded::instance();
        std::string log = emb.get_log();
        if (log.empty()) {
            cli::println("  内嵌 Tor 未运行, 无日志");
            return 0;
        }
        cli::println("  Tor 启动日志:");
        cli::println("{}", log);
        return 0;
    }

    // ---- tor circuits ----
    if (std::strcmp(sub, "circuits") == 0) {
        if (!g_tor_client.is_control_ready()) {
            cli::println(stderr, "[-] ControlPort 未连接。请先运行 tor start");
            return -1;
        }

        auto circuits = g_tor_client.get_circuits();
        if (circuits.empty()) {
            cli::println("  无活跃电路");
            return 0;
        }

        cli::println("  Tor 活跃电路 ({}):", circuits.size());
        for (auto& c : circuits) {
            cli::println("    {} | {:10} | {:15} | {}",
                         c.id.substr(0, 8), c.status, c.purpose,
                         c.path.size() > 40 ? c.path.substr(0, 40) + "..." : c.path);
        }
        return 0;
    }

    // ---- tor newid ----
    if (std::strcmp(sub, "newid") == 0) {
        if (!g_tor_client.is_control_ready()) {
            cli::println(stderr, "[-] ControlPort 未连接");
            return -1;
        }
        std::string resp = g_tor_client.control_command("SIGNAL NEWNYM");
        if (resp.find("250") != std::string::npos) {
            cli::println("[+] 新身份已请求 — 后续连接将使用新电路");
        } else {
            cli::println("[-] NEWNYM 请求失败");
        }
        return 0;
    }

    // ---- tor stop ----
    if (std::strcmp(sub, "stop") == 0) {
        auto& emb = chrono::client::tor::TorEmbedded::instance();
        emb.stop();
        g_tor_client.disconnect_all();
        cli::println("[+] Tor 已停止");
        return 0;
    }

    // ---- tor onion ----
    if (std::strcmp(sub, "onion") == 0) {
        if (g_tor_client.is_control_ready()) {
            std::string resp = g_tor_client.control_command("GETINFO address");
            auto pos = resp.find("address=");
            if (pos != std::string::npos) {
                auto end = resp.find('\n', pos);
                std::string addr = resp.substr(pos + 8, end - pos - 8);
                cli::println("  Onion 地址: {}", addr);
                return 0;
            }
        }
        cli::println("  Onion 地址: {}", mgr.my_i2p().empty() ? "(Tor 未连接)" : mgr.my_i2p());
        return 0;
    }

    cli::println(stderr, "未知 tor 子命令: {}", sub);
    return -1;
}

// ============================================================
// 命令注册
// ============================================================
// 主程序启动时自动尝试连接 Tor
void try_auto_connect_tor()
{
    if (g_tor_client.is_socks_ready()) return;
    g_tor_client.connect_socks("127.0.0.1", 9050);
    if (g_tor_client.is_socks_ready()) {
        g_tor_client.connect_control("127.0.0.1", 9051);
    }
}

extern "C" int init_cmd_tor(void)
{
    register_command("tor",
        "Tor 传输层管理 (start/status/circuits/newid/stop/onion)",
        "tor <start|status|circuits|newid|stop|onion>",
        cmd_tor);
    return 0;
}
