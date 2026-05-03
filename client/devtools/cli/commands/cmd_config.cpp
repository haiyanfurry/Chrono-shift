/**
 * cmd_config.cpp — 配置管理命令 (C++23 版本)
 */
#include "../devtools_cli.hpp"

#include <cstdlib>
#include "print_compat.h"
#include <string_view>

namespace cli = chrono::client::cli;

static int cmd_config(int argc, char** argv)
{
    if (argc < 1) {
        cli::println("用法:");
        cli::println("  config show                    - 查看客户端配置");
        cli::println("  config set <key> <value>       - 修改配置项");
        cli::println("    可用配置项:");
        cli::println("      host   - 服务器地址 (如 127.0.0.1)");
        cli::println("      port   - 服务器端口 (如 4443)");
        cli::println("      tls    - TLS 开关 (1/0)");
        cli::println("      verbose- 详细模式 (1/0)");
        return -1;
    }

    const char* subcmd = argv[0];

    if (std::strcmp(subcmd, "show") == 0) {
        cli::println("");
        cli::println("  ╔══════════════════════════════════════════════════════════╗");
        cli::println("  ║     客户端配置 (Config)                                  ║");
        cli::println("  ╚══════════════════════════════════════════════════════════╝");
        cli::println("");
        cli::println("  ┌───────────────────────┬────────────────────────────────┐");
        cli::println("  │ 配置项                │ 当前值                        │");
        cli::println("  ├───────────────────────┼────────────────────────────────┤");
        cli::println("  │ 服务器地址 (host)     │ {:<30} │", cli::g_cli_config.host);
        cli::println("  │ 服务器端口 (port)     │ {:<30} │", cli::g_cli_config.port);
        cli::println("  │ TLS 启用 (tls)        │ {:<30} │",
                     cli::g_cli_config.use_tls ? "是 (1)" : "否 (0)");
        cli::println("  │ 详细模式 (verbose)    │ {:<30} │",
                     cli::g_cli_config.verbose ? "开 (1)" : "关 (0)");
        cli::println("  │ 会话状态              │ {:<30} │",
                     cli::g_cli_config.session_logged_in ? "已登录" : "未登录");
        cli::println("  │ WebSocket 状态        │ {:<30} │",
                     cli::g_cli_config.ws_connected ? "已连接" : "未连接");
        cli::println("  └───────────────────────┴────────────────────────────────┘");
        cli::println("");
        cli::println("  环境变量:");
        auto env_host = std::getenv("CHRONO_HOST");
        auto env_port = std::getenv("CHRONO_PORT");
        auto env_tls  = std::getenv("CHRONO_TLS");
        cli::println("    CHRONO_HOST = {}", env_host ? env_host : "(未设置)");
        cli::println("    CHRONO_PORT = {}", env_port ? env_port : "(未设置)");
        cli::println("    CHRONO_TLS  = {}", env_tls  ? env_tls  : "(未设置)");
        return 0;

    } else if (std::strcmp(subcmd, "set") == 0) {
        if (argc < 3) {
            cli::println(stderr, "用法: config set <key> <value>");
            return -1;
        }
        const char* key = argv[1];
        const char* value = argv[2];

        if (std::strcmp(key, "host") == 0) {
            cli::g_cli_config.host = value;
            cli::println("[+] host 已设为: {}", cli::g_cli_config.host);
        } else if (std::strcmp(key, "port") == 0) {
            int p = std::atoi(value);
            if (p <= 0 || p > 65535) {
                cli::println(stderr, "[-] 无效端口: {} (1-65535)", value);
                return -1;
            }
            cli::g_cli_config.port = static_cast<uint16_t>(p);
            cli::println("[+] port 已设为: {}", cli::g_cli_config.port);
        } else if (std::strcmp(key, "tls") == 0) {
            cli::g_cli_config.use_tls = (std::atoi(value) != 0);
            cli::println("[+] tls 已设为: {}", cli::g_cli_config.use_tls ? "启用" : "禁用");
        } else if (std::strcmp(key, "verbose") == 0) {
            cli::g_cli_config.verbose = (std::atoi(value) != 0);
            cli::println("[+] verbose 已设为: {}", cli::g_cli_config.verbose ? "开" : "关");
        } else {
            cli::println(stderr, "[-] 未知配置项: {} (可用: host, port, tls, verbose)", key);
            return -1;
        }
        return 0;

    } else {
        cli::println(stderr, "未知 config 子命令: {}", subcmd);
        return -1;
    }
}

extern "C" int init_cmd_config(void)
{
    register_command("config",
        "配置管理 (show/set host/port/tls/verbose)",
        "config show | config set <key> <value>",
        cmd_config);
    return 0;
}
