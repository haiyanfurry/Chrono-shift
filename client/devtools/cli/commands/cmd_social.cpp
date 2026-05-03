/**
 * cmd_social.cpp — 社交功能 CLI 命令
 * I2P 好友系统: 请求/接受/拒绝/屏蔽/消息
 */
#include "../devtools_cli.hpp"
#include "print_compat.h"
#include <string>
#include <string_view>
#include <cstring>
#include <ctime>

#include "social/SocialManager.h"
#include <iostream>

namespace cli = chrono::client::cli;
using namespace chrono::client::social;

// ============================================================
// uid 命令
// ============================================================
static int cmd_uid(int argc, char** argv)
{
    auto& mgr = SocialManager::instance();

    if (argc < 1) {
        cli::println("用法:");
        cli::println("  uid set <name>    - 设置你的 UID");
        cli::println("  uid show          - 显示 UID 和 I2P 地址");
        return -1;
    }

    const char* sub = argv[0];

    if (std::strcmp(sub, "set") == 0) {
        if (argc < 2) {
            cli::println(stderr, "用法: uid set <name>");
            return -1;
        }
        mgr.set_uid(argv[1]);
        cli::println("[+] UID 已设置为: {}", argv[1]);

        std::string i2p = mgr.my_i2p();
        if (!i2p.empty()) {
            cli::println("    I2P 地址: {}", i2p);
        }
        return 0;
    }

    if (std::strcmp(sub, "show") == 0) {
        cli::println("  UID:       {}", mgr.my_uid().empty() ? "(未设置)" : mgr.my_uid());
        cli::println("  I2P 地址:  {}", mgr.my_i2p().empty() ? "(未连接 I2P)" : mgr.my_i2p());
        auto fl = mgr.friend_list();
        cli::println("  好友数量:  {}", fl.size());
        return 0;
    }

    cli::println(stderr, "未知 uid 子命令: {}", sub);
    return -1;
}

// ============================================================
// friend 命令 — 好友管理
// ============================================================
static int cmd_friend(int argc, char** argv)
{
    auto& mgr = SocialManager::instance();
    mgr.cleanup_expired_blocks();

    if (argc < 1) {
        cli::println("用法:");
        cli::println("  friend add <uid>           - 发送好友请求");
        cli::println("  friend list                - 好友列表");
        cli::println("  friend pending             - 待处理的请求");
        cli::println("  friend accept <uid>        - 接受好友请求");
        cli::println("  friend reject <uid>        - 拒绝 (屏蔽30分钟)");
        return -1;
    }

    const char* sub = argv[0];

    if (std::strcmp(sub, "add") == 0) {
        if (argc < 2) {
            cli::println(stderr, "用法: friend add <uid>");
            return -1;
        }
        if (mgr.my_uid().empty()) {
            cli::println(stderr, "[-] 请先设置 UID: uid set <name>");
            return -1;
        }
        const char* target = argv[1];
        if (std::strcmp(target, mgr.my_uid().c_str()) == 0) {
            cli::println(stderr, "[-] 不能添加自己为好友");
            return -1;
        }
        if (mgr.is_friend(target)) {
            cli::println("[!] {} 已经是你的好友", target);
            return 0;
        }
        if (mgr.is_blocked(target)) {
            cli::println("[-] {} 已被你屏蔽，请等待屏蔽过期", target);
            return -1;
        }

        mgr.add_pending_request(target, std::string(target) + ".b32.i2p",
                                 "Hi! I'm " + mgr.my_uid());
        cli::println("[+] 好友请求已发送给: {}", target);
        cli::println("    等待对方接受...");
        return 0;
    }

    if (std::strcmp(sub, "list") == 0) {
        auto fl = mgr.friend_list();
        if (fl.empty()) {
            cli::println("  暂无好友");
            return 0;
        }
        cli::println("  好友列表 ({}):", fl.size());
        for (auto& uid : fl) {
            bool blocked = mgr.is_blocked(uid);
            cli::println("    {} {}",
                uid, blocked ? "(已屏蔽)" : "");
        }
        return 0;
    }

    if (std::strcmp(sub, "pending") == 0) {
        auto pending = mgr.pending_requests();
        if (pending.empty()) {
            cli::println("  暂无待处理的好友请求");
            return 0;
        }
        cli::println("  [!] 待处理的好友请求 ({}):", pending.size());
        for (auto& req : pending) {
            char ts[32];
            time_t t = (time_t)req.timestamp;
            strftime(ts, sizeof(ts), "%H:%M:%S", localtime(&t));
            cli::println("    来自: {} ({}), 消息: {}, 时间: {}",
                req.from_uid, req.from_i2p, req.greeting, ts);
        }
        cli::println("  使用 friend accept <uid> 或 friend reject <uid> 处理");
        return 0;
    }

    if (std::strcmp(sub, "accept") == 0) {
        if (argc < 2) {
            cli::println(stderr, "用法: friend accept <uid>");
            return -1;
        }
        if (!mgr.has_pending_from(argv[1])) {
            cli::println(stderr, "[-] 没有来自 {} 的好友请求", argv[1]);
            return -1;
        }
        mgr.accept_request(argv[1]);
        cli::println("[+] {} 已成为你的好友!", argv[1]);
        return 0;
    }

    if (std::strcmp(sub, "reject") == 0) {
        if (argc < 2) {
            cli::println(stderr, "用法: friend reject <uid>");
            return -1;
        }
        if (!mgr.has_pending_from(argv[1])) {
            cli::println(stderr, "[-] 没有来自 {} 的好友请求", argv[1]);
            return -1;
        }
        mgr.reject_request(argv[1]);
        cli::println("[+] {} 已被拒绝并屏蔽 30 分钟", argv[1]);
        return 0;
    }

    cli::println(stderr, "未知 friend 子命令: {}", sub);
    return -1;
}

// ============================================================
// msg 命令 — 消息收发 (社交版)
// ============================================================
static int cmd_msg_social(int argc, char** argv)
{
    auto& mgr = SocialManager::instance();

    if (argc < 1) {
        cli::println("用法:");
        cli::println("  msg send <uid> <text>    - 发送消息给好友");
        cli::println("  msg inbox                - 查看收件箱");
        cli::println("  msg chat <uid>           - 交互式聊天模式 (输入 /quit 退出)");
        return -1;
    }

    const char* sub = argv[0];

    if (std::strcmp(sub, "send") == 0) {
        if (argc < 3) {
            cli::println(stderr, "用法: msg send <uid> <text>");
            return -1;
        }
        if (mgr.my_uid().empty()) {
            cli::println(stderr, "[-] 请先设置 UID");
            return -1;
        }
        if (!mgr.is_friend(argv[1])) {
            cli::println(stderr, "[-] {} 不是你的好友", argv[1]);
            return -1;
        }
        if (mgr.is_blocked(argv[1])) {
            cli::println(stderr, "[-] {} 已被屏蔽", argv[1]);
            return -1;
        }
        // 合并 text (从 argv[2] 开始的所有参数)
        std::string text;
        for (int i = 2; i < argc; i++) {
            if (i > 2) text += " ";
            text += argv[i];
        }
        mgr.add_message(mgr.my_uid(), argv[1], text);
        cli::println("[+] 消息已发送给 {}: {}", argv[1], text);
        return 0;
    }

    if (std::strcmp(sub, "inbox") == 0) {
        if (mgr.my_uid().empty()) {
            cli::println("  请先设置 UID");
            return 0;
        }
        auto pending = mgr.pending_requests();
        auto msgs = mgr.get_chat_history("");
        if (pending.empty() && msgs.empty()) {
            cli::println("  收件箱为空");
            return 0;
        }
        // 显示待处理请求
        if (!pending.empty()) {
            cli::println("  [!] {} 个待处理好友请求 (friend pending 查看)", pending.size());
        }
        // 显示最近消息
        if (!msgs.empty()) {
            cli::println("  最近消息:");
            size_t start = msgs.size() > 10 ? msgs.size() - 10 : 0;
            for (size_t i = start; i < msgs.size(); i++) {
                auto& m = msgs[i];
                char ts[16];
                time_t t = (time_t)m.timestamp;
                strftime(ts, sizeof(ts), "%H:%M", localtime(&t));
                cli::println("    [{}] {}: {}", ts, m.from_uid, m.text);
            }
        }
        return 0;
    }

    if (std::strcmp(sub, "chat") == 0) {
        if (argc < 2) {
            cli::println(stderr, "用法: msg chat <uid>");
            return -1;
        }
        if (mgr.my_uid().empty()) {
            cli::println(stderr, "[-] 请先设置 UID");
            return -1;
        }
        const char* peer = argv[1];
        if (!mgr.is_friend(peer)) {
            cli::println(stderr, "[-] {} 不是你的好友。先发送好友请求: friend add {}", peer, peer);
            return -1;
        }

        cli::println("=== 与 {} 的聊天 (输入 /quit 退出, /history 查看历史) ===", peer);

        // 显示历史
        auto history = mgr.get_chat_history(peer);
        for (auto& m : history) {
            char ts[16];
            time_t t = (time_t)m.timestamp;
            strftime(ts, sizeof(ts), "%H:%M", localtime(&t));
            cli::println("[{}] {}: {}", ts, m.from_uid, m.text);
        }

        std::string line;
        while (true) {
            cli::print("[{}]> ", peer);
            std::fflush(stdout);
            if (!std::getline(std::cin, line)) break;
            if (line.empty()) continue;
            if (line == "/quit" || line == "/exit") break;
            if (line == "/history") {
                auto hist = mgr.get_chat_history(peer);
                for (auto& m : hist) {
                    char ts[16];
                    time_t t = (time_t)m.timestamp;
                    strftime(ts, sizeof(ts), "%H:%M", localtime(&t));
                    cli::println("[{}] {}: {}", ts, m.from_uid, m.text);
                }
                continue;
            }
            mgr.add_message(mgr.my_uid(), peer, line);
            cli::println("  [sent] {}", line);
        }
        cli::println("  已退出聊天");
        return 0;
    }

    cli::println(stderr, "未知 msg 子命令: {}", sub);
    return -1;
}

// ============================================================
// 命令注册
// ============================================================
extern "C" int init_cmd_social(void)
{
    register_command("uid",
        "UID 管理 (set/show)",
        "uid set <name> | uid show",
        cmd_uid);

    register_command("friend",
        "好友管理 (add/list/pending/accept/reject)",
        "friend <add|list|pending|accept|reject> [uid]",
        cmd_friend);

    register_command("msg",
        "消息收发 (send/inbox/chat)",
        "msg <send|inbox|chat> [uid] [text]",
        cmd_msg_social);

    return 0;
}
