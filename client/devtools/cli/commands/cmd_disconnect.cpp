/**
 * cmd_disconnect.cpp — 断开当前连接命令
 * 对应 debug_cli.c:1047 cmd_disconnect
 *
 * C++23 转换: std::println, namespace cli
 */
#include "../devtools_cli.hpp"

#include <print>     // std::println

namespace cli = chrono::client::cli;

/* ============================================================
 * tls_close (由 core 或外部库提供)
 * ============================================================ */
extern "C" void tls_close(void* ssl);

/* ============================================================
 * disconnect 命令 - 断开当前连接
 * ============================================================ */
static int cmd_disconnect(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    /* 关闭 WebSocket 连接 (如果有) */
    if (cli::g_cli_config.ws_connected && cli::g_cli_config.ws_ssl) {
        tls_close(cli::g_cli_config.ws_ssl);
        cli::g_cli_config.ws_ssl = nullptr;
        cli::g_cli_config.ws_connected = false;
    }

    std::println("[+] 连接状态已重置");
    return 0;
}

extern "C" int init_cmd_disconnect(void)
{
    register_command("disconnect",
        "断开当前连接",
        "disconnect",
        cmd_disconnect);
    return 0;
}
