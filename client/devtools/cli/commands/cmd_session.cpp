/**
 * cmd_session.cpp — 会话管理命令
 *
 * C++23 重写: 使用 std::println, std::string, extern "C" 兼容层
 * 原 cmd_session.c 中的 init_cmd_session 在此实现。
 */

#include "print_compat.h"
#include <cstdio>      // std::snprintf, std::puts
#include <cstring>     // std::strcmp, std::strlen

#include "devtools_cli.hpp"

namespace cli = chrono::client::cli;

// ============================================================
// 前置声明
// ============================================================
extern int http_request(const char* method, const char* path,
                        const char* headers, const char* body,
                        char* response, size_t response_size);

// ============================================================
// cmd_session — 会话管理命令
// ============================================================
static int cmd_session(int argc, char** argv)
{
    if (argc < 1) {
        cli::println("用法:");
        cli::println("  session login <token>           - 设置会话令牌");
        cli::println("  session logout                  - 清除会话");
        cli::println("  session status                  - 查看会话状态");
        return -1;
    }

    const char* subcmd = argv[0];

    if (std::strcmp(subcmd, "login") == 0) {
        if (argc < 2) {
            cli::println(stderr, "用法: session login <jwt_token>");
            return -1;
        }
        const char* token = argv[1];
        cli::g_cli_config.session_token   = token;
        cli::g_cli_config.session_logged_in = true;
        cli::println("[+] 会话令牌已设置 ({} 字符)", std::strlen(token));
        return 0;

    } else if (std::strcmp(subcmd, "logout") == 0) {
        cli::g_cli_config.session_token.clear();
        cli::g_cli_config.session_logged_in = false;
        cli::println("[+] 会话已清除");
        return 0;

    } else if (std::strcmp(subcmd, "status") == 0) {
        cli::println("[*] 会话状态:");
        cli::println("    登录状态:   {}", cli::g_cli_config.session_logged_in
                     ? "已登录" : "未登录");
        if (cli::g_cli_config.session_logged_in) {
            cli::println("    令牌前缀:   {}...",
                         cli::g_cli_config.session_token.substr(0, 20));
            cli::println("    令牌长度:   {} 字符",
                         cli::g_cli_config.session_token.size());
        }
        return 0;

    } else {
        cli::println(stderr, "未知 session 子命令: {}", subcmd);
        return -1;
    }
}

// ============================================================
// extern "C" 入口 — 供 init_commands.cpp 调用
// ============================================================
extern "C" int init_cmd_session(void)
{
    register_command("session", "会话管理 (登录/登出/状态)",
                     "session login <token> | logout | status",
                     cmd_session);
    return 0;
}
