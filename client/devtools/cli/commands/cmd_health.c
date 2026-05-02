/**
 * cmd_health.c - 健康检查命令
 * 对应 debug_cli.c:510 cmd_health
 */
#include "../devtools_cli.h"
#include <stdio.h>
#include <string.h>

/* ============================================================
 * http_request / http_get_status / http_get_body 
 * 这些由 core/net_http.c 提供 (后续 Phase 3 实现)
 * 当前引用外部符号
 * ============================================================ */
extern int http_request(const char* method, const char* path,
                        const char* body, const char* content_type,
                        char* response, size_t resp_size);
extern int http_get_status(const char* response);
extern const char* http_get_body(const char* response);

#define BUFFER_SIZE 65536

/** health - 检查服务器健康状态 */
static int cmd_health(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    printf("[*] 检查服务器健康状态: %s:%d\n", g_config.host, g_config.port);

    char response[BUFFER_SIZE] = {0};
    if (http_request("GET", "/api/health", NULL, NULL,
                      response, sizeof(response)) != 0) {
        printf("[-] 服务器未响应\n");
        return -1;
    }

    int status = http_get_status(response);
    const char* body = http_get_body(response);

    printf("[*] HTTP %d\n", status);

    if (status >= 200 && status < 300) {
        printf("[+] 服务器运行正常\n");
        if (strlen(body) > 0) {
            printf("    响应: ");
            print_json(body, 4);
        }
        return 0;
    } else if (status >= 400) {
        printf("[-] 服务器返回错误\n");
        if (strlen(body) > 0) {
            printf("    响应: ");
            print_json(body, 4);
        }
        return -1;
    } else {
        printf("[?] 未知状态码\n");
        if (strlen(body) > 0) {
            printf("    响应: %s\n", body);
        }
        return -1;
    }
}

void init_cmd_health(void)
{
    register_command("health",
                     "检查服务器健康状态",
                     "health",
                     cmd_health);
}
