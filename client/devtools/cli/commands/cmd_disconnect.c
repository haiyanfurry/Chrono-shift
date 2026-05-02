/**
 * cmd_disconnect.c — 断开当前连接命令
 * 对应 debug_cli.c:1047 cmd_disconnect
 */
#include "../devtools_cli.h"

/* ============================================================
 * tls_close (由 core 或外部库提供)
 * ============================================================ */
extern void tls_close(void* ssl);

/* ============================================================
 * disconnect 命令 - 断开当前连接
 * ============================================================ */
static int cmd_disconnect(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    /* 关闭 WebSocket 连接 (如果有) */
    if (g_config.ws_connected && g_config.ws_ssl) {
        tls_close(g_config.ws_ssl);
        g_config.ws_ssl = NULL;
        g_config.ws_connected = 0;
    }

    printf("[+] 连接状态已重置\n");
    return 0;
}

int init_cmd_disconnect(void)
{
    register_command("disconnect",
        "断开当前连接",
        "disconnect",
        cmd_disconnect);
    return 0;
}
