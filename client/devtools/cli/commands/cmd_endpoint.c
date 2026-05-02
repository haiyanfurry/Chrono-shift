/**
 * cmd_endpoint.c - API 端点测试命令
 * 对应 debug_cli.c:552 cmd_endpoint
 */
#include "../devtools_cli.h"
#include <stdio.h>
#include <string.h>

extern int http_request(const char* method, const char* path,
                        const char* body, const char* content_type,
                        char* response, size_t resp_size);
extern int http_get_status(const char* response);
extern const char* http_get_body(const char* response);

#define BUFFER_SIZE 65536

/** endpoint - 测试 API 端点 */
static int cmd_endpoint(int argc, char** argv)
{
    if (argc < 1) {
        fprintf(stderr, "用法: endpoint <path> [method] [body]\n");
        fprintf(stderr, "  path   - API 路径, 如 /api/user/profile?id=1\n");
        fprintf(stderr, "  method - HTTP 方法 (GET/POST/PUT/DELETE, 默认 GET)\n");
        fprintf(stderr, "  body   - POST/PUT 请求体 (JSON 字符串)\n");
        return -1;
    }

    const char* path   = argv[0];
    const char* method = (argc >= 2) ? argv[1] : "GET";
    const char* body   = (argc >= 3) ? argv[2] : NULL;

    printf("[*] %s %s%s%s\n", method, path,
           body ? " body: " : "", body ? body : "");

    char response[BUFFER_SIZE] = {0};
    if (http_request(method, path, body, "application/json",
                      response, sizeof(response)) != 0) {
        printf("[-] 请求失败\n");
        return -1;
    }

    int status = http_get_status(response);
    const char* resp_body = http_get_body(response);

    printf("[*] HTTP %d\n", status);
    if (strlen(resp_body) > 0) {
        print_json(resp_body, 0);
    }
    return (status >= 200 && status < 300) ? 0 : -1;
}

void init_cmd_endpoint(void)
{
    register_command("endpoint",
                     "测试 API 端点",
                     "endpoint <path> [method] [body]",
                     cmd_endpoint);
}
