/**
 * cmd_storage.cpp — 安全存储命令
 * 对应 debug_cli.c:2388 cmd_storage
 *
 * C++23 转换: std::println, std::string_view, namespace cli
 */
#include "../devtools_cli.hpp"

#include <cstdlib>   // std::getenv
#include <print>     // std::println
#include <string>    // std::string
#include <string_view> // std::string_view

namespace cli = chrono::client::cli;

/* ============================================================
 * storage 命令 - 本地安全存储查看
 * ============================================================ */
static int cmd_storage(int argc, char** argv)
{
    if (argc < 1) {
        std::println("用法:");
        std::println("  storage list                   - 列出本地安全存储内容");
        std::println("  storage get <key>              - 读取安全存储条目");
        return -1;
    }

    /* 初始化存储路径 */
    if (cli::g_cli_config.storage_path.empty()) {
#ifdef _WIN32
        const char* appdata = std::getenv("APPDATA");
        cli::g_cli_config.storage_path = std::string(appdata ? appdata : ".")
            + "/Chrono-shift/secure";
#else
        const char* home = std::getenv("HOME");
        cli::g_cli_config.storage_path = std::string(home ? home : ".")
            + "/.chrono-shift/secure";
#endif
    }

    const std::string_view subcmd = argv[0];

    if (subcmd == "list") {
        std::println("");
        std::println("  ╔══════════════════════════════════════════════════════════╗");
        std::println("  ║     本地安全存储 (Secure Storage)                        ║");
        std::println("  ╚══════════════════════════════════════════════════════════╝");
        std::println("");
        std::println("  存储路径: {}", cli::g_cli_config.storage_path);
        std::println("");
        std::println("  存储内容 (模拟):");
        std::println("  ┌──────────┬────────────────────────────────────────────┐");
        std::println("  │ 键名     │ 值                                         │");
        std::println("  ├──────────┼────────────────────────────────────────────┤");
        if (cli::g_cli_config.session_logged_in) {
            std::println("  │ token    │ {:<42} │", cli::g_cli_config.session_token);
        } else {
            std::println("  │ token    │ {:<42} │", "(空)");
        }
        if (cli::g_cli_config.session_logged_in) {
            std::println("  │ user_id  │ {:<42} │", "1");
        } else {
            std::println("  │ user_id  │ {:<42} │", "(空)");
        }
        std::println("  │ device   │ chrono-cli (当前工具)                      │");
        std::println("  └──────────┴────────────────────────────────────────────┘");
        std::println("");
        std::println("  说明: 生产环境中使用 AES-256-GCM 加密存储");
        std::println("  实现:  client/security/src/secure_storage.rs (Rust FFI)");
        std::println("  路径:  {}/*.chrono_*", cli::g_cli_config.storage_path);
        return 0;

    } else if (subcmd == "get") {
        if (argc < 2) {
            std::println(stderr, "用法: storage get <key>");
            return -1;
        }
        const std::string key = argv[1];

        std::println("[*] 读取安全存储: key={}", key);
        if (key == "token") {
            if (cli::g_cli_config.session_logged_in) {
                std::println("[+] token = {}", cli::g_cli_config.session_token);
            } else {
                std::println("[*] token = (未设置, 请先 session login)");
            }
        } else {
            std::println("[-] 键 '{}' 不存在于本地存储", key);
            std::println("[*] 可用键: token, user_id, device");
            return -1;
        }
        return 0;

    } else {
        std::println(stderr, "未知 storage 子命令: {}", subcmd);
        return -1;
    }
}

extern "C" int init_cmd_storage(void)
{
    register_command("storage",
        "安全存储管理 (list/get)",
        "storage list | storage get <key>",
        cmd_storage);
    return 0;
}
