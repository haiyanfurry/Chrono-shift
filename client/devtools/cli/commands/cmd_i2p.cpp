/**
 * cmd_i2p.cpp — I2P 网络管理命令
 * 连接 SAM 桥接, 查看状态, 本地模拟模式
 */
#include "../devtools_cli.hpp"
#include "print_compat.h"
#include <string>
#include <cstring>

#include "i2p/SamClient.h"
#include "i2p/I2pdEmbedded.h"
#include "social/SocialManager.h"
#include <thread>
#include <chrono>

namespace cli = chrono::client::cli;
using namespace chrono::client::i2p;
using namespace chrono::client::social;

static SamClient g_i2p_client;

static int cmd_i2p(int argc, char** argv)
{
    auto& mgr = SocialManager::instance();

    if (argc < 1) {
        cli::println("I2P — 备选传输层");
        cli::println("");
        cli::println("  i2p start [mode]   - 启动 I2P (嵌入/外部/模拟)");
        cli::println("  i2p status         - 查看 I2P 连接状态");
        cli::println("  i2p stop           - 断开 I2P 连接");
        cli::println("");
        cli::println("  模式: embedded (内嵌i2pd) | external (SAM桥接) | auto");
        return -1;
    }

    const char* sub = argv[0];

    if (std::strcmp(sub, "start") == 0) {
        const char* mode = (argc >= 2) ? argv[1] : "auto";

        // 嵌入模式: 启动内嵌 i2pd 路由器
        auto try_embedded = [&]() -> bool {
            auto& emb = chrono::client::i2p::I2pdEmbedded::instance();
            if (emb.is_ready()) {
                cli::println("[!] i2pd 已运行, 地址: {}", emb.our_address());
                return true;
            }
            cli::println("[*] 启动内嵌 i2pd 路由器...");
            if (!emb.start("./i2p_data", 7656)) {
                cli::println("{}[!] 内嵌 i2pd 启动失败{}", cli::COLOR_YELLOW, cli::COLOR_RESET);
                return false;
            }
            // 等待集成完成
            for (int i = 0; i < 60; i++) {
                if (emb.is_ready()) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
            if (emb.is_ready()) {
                cli::println("{}[+] i2pd 内嵌运行中{}", cli::COLOR_GREEN, cli::COLOR_RESET);
                cli::println("    地址: {}", emb.our_address());
                cli::println("    节点: {}, 隧道: {}", emb.known_nodes(), emb.tunnel_count());
                mgr.set_i2p_addr(emb.our_address());
                return true;
            }
            cli::println("{}[!] i2pd 集成超时{}", cli::COLOR_YELLOW, cli::COLOR_RESET);
            return false;
        };

        if (std::strcmp(mode, "embedded") == 0) return try_embedded() ? 0 : -1;

        // 外部/自动模式
        if (std::strcmp(mode, "auto") == 0 && try_embedded()) return 0;

        // 回退到外部 SAM 桥接
        if (g_i2p_client.is_connected()) {
            cli::println("[!] 已连接, I2P 地址: {}", g_i2p_client.our_destination());
            return 0;
        }

        cli::println("[*] 正在连接 I2P SAM 桥接 (127.0.0.1:7656)...");
        if (!g_i2p_client.connect("127.0.0.1", 7656)) {
            cli::println("{}[!] I2P 路由器不可用，使用本地模拟模式{}",
                         cli::COLOR_YELLOW, cli::COLOR_RESET);
            if (!g_i2p_client.create_session("chrono")) {
                cli::println(stderr, "[-] 本地模拟模式初始化失败");
                return -1;
            }
        }

        cli::println("{}[+] I2P 已就绪{}", cli::COLOR_GREEN, cli::COLOR_RESET);
        cli::println("    模式: {}", g_i2p_client.is_connected() ? "I2P SAM" : "本地模拟");
        cli::println("    地址: {}", g_i2p_client.our_destination());
        mgr.set_i2p_addr(g_i2p_client.our_destination());
        return 0;
    }

    if (std::strcmp(sub, "status") == 0) {
        auto& emb = chrono::client::i2p::I2pdEmbedded::instance();

        cli::println("  I2P 状态:");
        if (emb.is_ready()) {
            cli::println("    模式:   {}内嵌 i2pd (运行中){}", cli::COLOR_GREEN, cli::COLOR_RESET);
            cli::println("    地址:   {}", emb.our_address());
            cli::println("    节点:   {}", emb.known_nodes());
            cli::println("    隧道:   {}", emb.tunnel_count());
            cli::println("    运行:   {}s", emb.uptime_seconds());
        } else {
            cli::println("    连接:   {}", g_i2p_client.is_connected() ? "已连接" : "未连接");
            if (g_i2p_client.is_connected() && g_i2p_client.is_local_mode())
                cli::println("    模式:   {}本地模拟 (I2P 路由器未运行){}",
                    cli::COLOR_YELLOW, cli::COLOR_RESET);
            cli::println("    地址:   {}", g_i2p_client.our_destination().empty()
                              ? "-" : g_i2p_client.our_destination());
        }
        cli::println("    UID:    {}", mgr.my_uid().empty() ? "(未设置)" : mgr.my_uid());
        cli::println("    好友:   {}", mgr.friend_list().size());
        auto pending = mgr.pending_requests();
        if (!pending.empty())
            cli::println("    {}[!] {} 个待处理好友请求 (friend pending){}",
                         cli::COLOR_YELLOW, pending.size(), cli::COLOR_RESET);
        return 0;
    }

    // ---- i2p log ----
    if (std::strcmp(sub, "log") == 0) {
        auto& emb = chrono::client::i2p::I2pdEmbedded::instance();
        std::string log = emb.get_log();
        if (log.empty()) { cli::println("  i2pd 未运行, 无日志"); return 0; }
        cli::println("{}", log);
        return 0;
    }

    // ---- i2p stop ----
    if (std::strcmp(sub, "stop") == 0) {
        auto& emb = chrono::client::i2p::I2pdEmbedded::instance();
        emb.stop();
        g_i2p_client.disconnect();
        cli::println("[+] I2P 已停止");
        return 0;
    }

    if (std::strcmp(sub, "lookup") == 0) {
        if (argc < 2) {
            cli::println(stderr, "用法: i2p lookup <b32.i2p>");
            return -1;
        }
        std::string dest = g_i2p_client.naming_lookup(argv[1]);
        cli::println("    解析结果: {}", dest);
        return 0;
    }

    cli::println(stderr, "未知 i2p 子命令: {}", sub);
    return -1;
}

extern "C" int init_cmd_i2p(void)
{
    register_command("i2p",
        "I2P 网络管理 (start/status/stop/lookup)",
        "i2p <start|status|stop|lookup>",
        cmd_i2p);
    return 0;
}
