/**
 * cmd_connect.cpp — 连接目标服务器命令
 * 对应 debug_cli.c:2991 内联 connect 处理
 *
 * C++23 转换: std::println, std::string, namespace cli
 */
#include "../devtools_cli.hpp"

#include <cstdlib>   // std::atoi
#include <print>     // std::println
#include <string>    // std::string

namespace cli = chrono::client::cli;

/* ============================================================
 * connect 命令 - 设置目标服务器
 * ============================================================ */
static int cmd_connect(int argc, char** argv)
{
    if (argc < 2) {
        std::println(stderr, "用法: connect <host> <port> [tls]");
        std::println(stderr, "  连接到指定服务器");
        std::println(stderr, "  示例: connect 127.0.0.1 4443 tls");
        return -1;
    }

    const std::string host = argv[0];
    int port = std::atoi(argv[1]);

    if (port <= 0 || port > 65535) {
        std::println(stderr, "[-] 无效端口: {} (1-65535)", port);
        return -1;
    }

    cli::g_cli_config.host = host;
    cli::g_cli_config.port = port;

    if (argc >= 3) {
        const std::string tls_opt = argv[2];
        cli::g_cli_config.use_tls = (tls_opt == "tls" || tls_opt == "1");
    } else {
        cli::g_cli_config.use_tls = false;
    }

    std::println("[*] 目标服务器: {}:{} ({})",
                 cli::g_cli_config.host,
                 cli::g_cli_config.port,
                 cli::g_cli_config.use_tls ? "HTTPS" : "HTTP");
    return 0;
}

extern "C" int init_cmd_connect(void)
{
    register_command("connect",
        "连接到指定服务器",
        "connect <host> <port> [tls]",
        cmd_connect);
    return 0;
}
