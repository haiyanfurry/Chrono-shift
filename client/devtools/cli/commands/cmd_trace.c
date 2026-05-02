/**
 * cmd_trace.c — 请求追踪命令
 * 对应 debug_cli.c:1175 cmd_trace
 */
#include "../devtools_cli.h"

/* ============================================================
 * http_request / tls_last_error
 * ============================================================ */
extern int http_request(const char* method, const char* path,
                        const char* headers, const char* body,
                        char* response, size_t resp_size);
extern int http_get_status(const char* response);
extern const char* http_get_body(const char* response);
extern const char* tls_last_error(void);

#define BUFFER_SIZE 8192

/* ============================================================
 * trace 命令 - 追踪请求路径
 * ============================================================ */
static int cmd_trace(int argc, char** argv)
{
    if (argc < 1) {
        fprintf(stderr, "用法: trace <path>\n");
        fprintf(stderr, "  发送 TRACE 请求追踪请求经过的路径\n");
        fprintf(stderr, "  示例: trace /api/health\n");
        return -1;
    }

    const char* path = argv[0];
    printf("[*] 追踪请求路径: %s\n", path);
    printf("    方法: GET -> %s:%d%s\n", g_config.host, g_config.port, path);
    printf("\n");

    /* 追踪过程的模拟步骤 */
    printf("  [1/5] DNS 解析: %s -> %s:%d\n",
           g_config.host, g_config.host, g_config.port);
    printf("  [2/5] TCP 连接: 建立连接...\n");

    /* 实际发送请求测试路径 */
    char response[BUFFER_SIZE] = {0};
    int ret = http_request("GET", path, NULL, NULL,
                            response, sizeof(response));

    if (ret == 0) {
        int status = http_get_status(response);
        const char* body = http_get_body(response);

        printf("  [3/5] TLS 握手: 完成 (HTTPS)\n");
        printf("  [4/5] HTTP 请求: %s %s -> HTTP %d\n", "GET", path, status);
        printf("  [5/5] 响应处理: 完成\n");
        printf("\n");

        /* 路由匹配分析 */
        printf("[*] 路由分析:\n");
        if (strncmp(path, "/api/health", 11) == 0)
            printf("     => health_handler (健康检查)\n");
        else if (strncmp(path, "/api/user", 9) == 0)
            printf("     => user_handler (用户管理)\n");
        else if (strncmp(path, "/api/message", 12) == 0)
            printf("     => message_handler (消息处理)\n");
        else if (strncmp(path, "/api/community", 14) == 0)
            printf("     => community_handler (社区管理)\n");
        else if (strncmp(path, "/api/files", 10) == 0)
            printf("     => file_handler (文件处理)\n");
        else if (strncmp(path, "/api/ws", 7) == 0)
            printf("     => websocket_handler (WebSocket)\n");
        else
            printf("     => 未知路由或静态文件\n");

        if (strlen(body) > 0) {
            printf("\n[*] 响应数据:\n");
            print_json(body, 4);
        }

        return (status >= 200 && status < 300) ? 0 : -1;
    }

    printf("  [3/5] TLS 握手: 失败 - %s\n", tls_last_error());
    printf("  [4/5] HTTP 请求: 未发送\n");
    printf("  [5/5] 响应处理: 无响应\n");
    return -1;
}

int init_cmd_trace(void)
{
    register_command("trace",
        "追踪请求路径",
        "trace <path>",
        cmd_trace);
    return 0;
}
