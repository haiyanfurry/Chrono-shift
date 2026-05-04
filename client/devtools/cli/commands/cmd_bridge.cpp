/**
 * cmd_bridge.cpp — 跨传输桥接命令 (I2P ↔ Tor)
 *
 * 借鉴 RetroShare F2F 信任网模型:
 *   桥接节点同时连接两个网络, 在信任好友间转发消息。
 */
#include "../devtools_cli.hpp"
#include "print_compat.h"
#include "glue/TransportBridge.h"
#include "social/SocialManager.h"
#include <string>
#include <cstring>

namespace cli = chrono::client::cli;
using namespace chrono::glue;
using namespace chrono::client::social;

static int cmd_bridge(int argc, char** argv)
{
    auto& bridge = TransportBridge::instance();

    if (argc < 1) {
        cli::println("Bridge — 跨传输桥接 (I2P ↔ Tor)");
        cli::println("");
        cli::println("  bridge status      - 桥接状态");
        cli::println("  bridge add <uid> <tor|i2p> <addr>  - 添加跨传输路由");
        cli::println("  bridge list        - 已注册的跨传输路由");
        cli::println("  bridge queue       - 待转发消息队列");
        cli::println("  bridge announce    - 宣告桥接节点双地址");
        return -1;
    }

    const char* sub = argv[0];

    if (std::strcmp(sub, "status") == 0) {
        cli::println("  桥接状态:");
        cli::println("    I2P 地址:   {}", bridge.i2p_address().empty()
                          ? "(未设置)" : bridge.i2p_address());
        cli::println("    Tor 地址:   {}", bridge.tor_address().empty()
                          ? "(未设置)" : bridge.tor_address());
        cli::println("    待转发消息: {}", bridge.queue_size());
        return 0;
    }

    if (std::strcmp(sub, "add") == 0) {
        if (argc < 4) {
            cli::println(stderr, "用法: bridge add <uid> <tor|i2p> <addr>");
            cli::println(stderr, "示例: bridge add alice tor xyz.onion");
            cli::println(stderr, "示例: bridge add bob i2p abc.b32.i2p");
            return -1;
        }
        const char* uid = argv[1];
        const char* net = argv[2];
        const char* addr = argv[3];

        std::string tor_addr, i2p_addr;
        if (std::strcmp(net, "tor") == 0) tor_addr = addr;
        else if (std::strcmp(net, "i2p") == 0) i2p_addr = addr;
        else {
            cli::println(stderr, "网络类型: tor 或 i2p");
            return -1;
        }

        bridge.add_route(uid, tor_addr, i2p_addr);
        cli::println("[+] 路由已添加: {} → {} ({})", uid, addr, net);
        return 0;
    }

    if (std::strcmp(sub, "list") == 0) {
        // 显示所有已知路由 (遍历 SocialManager 好友)
        auto& mgr = SocialManager::instance();
        auto friends = mgr.friend_list();
        cli::println("  跨传输路由 ({} 个好友):", friends.size());
        for (auto& uid : friends) {
            auto* route = bridge.find_route(uid);
            if (route) {
                cli::println("    {} → {} (via {})",
                    uid, route->target_addr,
                    route->dst_network == TransportKind::Tor ? "Tor" : "I2P");
            } else {
                cli::println("    {} — 直连 (同网络)", uid);
            }
        }
        return 0;
    }

    if (std::strcmp(sub, "queue") == 0) {
        cli::println("  待转发队列: {} 条消息", bridge.queue_size());
        return 0;
    }

    if (std::strcmp(sub, "announce") == 0) {
        bridge.announce();
        cli::println("[+] 桥接节点双地址已宣告");
        return 0;
    }

    cli::println(stderr, "未知 bridge 子命令: {}", sub);
    return -1;
}

extern "C" int init_cmd_bridge(void)
{
    register_command("bridge",
        "跨传输桥接 (I2P↔Tor) — RetroShare F2F 信任网",
        "bridge <status|add|list|queue|announce>",
        cmd_bridge);
    return 0;
}
