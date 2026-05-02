/**
 * cmd_connect.c — 连接目标服务器命令
 * 对应 debug_cli.c:2991 内联 connect 处理
 */
#include "../devtools_cli.h"

/* ============================================================
 * connect 命令 - 设置目标服务器
 * ============================================================ */
static int cmd_connect(int argc, char** argv)
{
    if (argc < 2) {
        fprintf(stderr, "用法: connect <host> <port> [tls]\n");
        fprintf(stderr, "  连接到指定服务器\n");
        fprintf(stderr, "  示例: connect 127.0.0.1 4443 tls\n");
        return -1;
    }

    const char* host = argv[0];
    int port = atoi(argv[1]);

    if (port <= 0 || port > 65535) {
        fprintf(stderr, "[-] 无效端口: %d (1-65535)\n", port);
        return -1;
    }

    snprintf(g_config.host, sizeof(g_config.host), "%s", host);
    g_config.port = port;

    if (argc >= 3 && (strcmp(argv[2], "tls") == 0 || strcmp(argv[2], "1") == 0)) {
        g_config.use_tls = 1;
    } else {
        g_config.use_tls = 0;
    }

    printf("[*] 目标服务器: %s:%d (%s)\n",
           g_config.host, g_config.port,
           g_config.use_tls ? "HTTPS" : "HTTP");
    return 0;
}

int init_cmd_connect(void)
{
    register_command("connect",
        "连接到指定服务器",
        "connect <host> <port> [tls]",
        cmd_connect);
    return 0;
}
