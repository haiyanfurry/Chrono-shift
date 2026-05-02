/**
 * cmd_tls.c — TLS 连接信息命令
 * 对应 debug_cli.c:1065 cmd_tls_info
 */
#include "../devtools_cli.h"

/* ============================================================
 * tls_client 函数 (由 core 或外部库提供)
 * ============================================================ */
extern int tls_client_init(const char* cert_dir);
extern int tls_client_connect(void** ssl, const char* host, unsigned short port);
extern void tls_close(void* ssl);
extern const char* tls_last_error(void);
extern void tls_get_info(void* ssl, char* buf, size_t buf_size);

/* ============================================================
 * tls-info 命令 - 获取 TLS 连接信息
 * ============================================================ */
static int cmd_tls_info(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    printf("[*] 获取 TLS 连接信息: %s:%d\n", g_config.host, g_config.port);

    /* 建立临时 TLS 连接来获取信息 */
    void* ssl = NULL;
    if (tls_client_init(NULL) != 0) {
        fprintf(stderr, "[-] TLS 客户端初始化失败: %s\n", tls_last_error());
        return -1;
    }
    if (tls_client_connect(&ssl, g_config.host, (unsigned short)g_config.port) < 0) {
        fprintf(stderr, "[-] 无法连接到 %s:%d: %s\n",
                g_config.host, g_config.port, tls_last_error());
        return -1;
    }

    char info[2048] = {0};
    tls_get_info(ssl, info, sizeof(info));
    printf("[+] TLS 连接信息:\n%s\n", info);

    tls_close(ssl);
    return 0;
}

int init_cmd_tls_info(void)
{
    register_command("tls-info",
        "显示 TLS 连接信息",
        "tls-info",
        cmd_tls_info);
    return 0;
}
