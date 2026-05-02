/**
 * cmd_msg.c - 消息操作命令
 * 对应 debug_cli.c:1998 cmd_msg
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

/** msg - 消息操作命令 */
static int cmd_msg(int argc, char** argv)
{
    if (argc < 1) {
        printf("用法:\n");
        printf("  msg list <user_id> [limit] [offset]  - 列出用户消息\n");
        printf("  msg get <msg_id>                     - 获取消息详情\n");
        printf("  msg send <to_user_id> <text>         - 发送测试消息\n");
        return -1;
    }

    const char* subcmd = argv[0];

    if (strcmp(subcmd, "list") == 0) {
        if (argc < 2) {
            fprintf(stderr, "用法: msg list <user_id> [limit] [offset]\n");
            return -1;
        }
        const char* uid = argv[1];
        const char* limit = (argc >= 3) ? argv[2] : "50";
        const char* offset = (argc >= 4) ? argv[3] : "0";

        char path[512];
        snprintf(path, sizeof(path), "/api/message/list?user_id=%s&limit=%s&offset=%s",
                 uid, limit, offset);
        printf("[*] 获取消息列表: user_id=%s, limit=%s, offset=%s\n", uid, limit, offset);

        char response[BUFFER_SIZE] = {0};
        if (http_request("GET", path, NULL, NULL, response, sizeof(response)) != 0) {
            printf("[-] 请求失败\n"); return -1;
        }
        int status = http_get_status(response);
        const char* body = http_get_body(response);
        if (status >= 200 && status < 300) {
            printf("[+] 消息列表 (HTTP %d):\n", status);
            if (strlen(body) > 0) print_json(body, 0);
            return 0;
        } else {
            printf("[-] HTTP %d\n", status);
            if (strlen(body) > 0) print_json(body, 4);
            return -1;
        }

    } else if (strcmp(subcmd, "get") == 0) {
        if (argc < 2) { fprintf(stderr, "用法: msg get <msg_id>\n"); return -1; }
        printf("[-] msg get 需要服务器实现 GET /api/message/get?id=X 端点\n");
        return -1;

    } else if (strcmp(subcmd, "send") == 0) {
        if (argc < 3) { fprintf(stderr, "用法: msg send <to_user_id> <text>\n"); return -1; }
        const char* to_uid = argv[1];
        const char* text = argv[2];

        char body[2048];
        snprintf(body, sizeof(body),
            "{\"to_user_id\":%s,\"content\":\"%s\"}", to_uid, text);
        printf("[*] 发送消息: to=%s, text=%s\n", to_uid, text);

        char response[BUFFER_SIZE] = {0};
        if (http_request("POST", "/api/message/send", body, "application/json",
                          response, sizeof(response)) != 0) {
            printf("[-] 请求失败\n"); return -1;
        }
        int status = http_get_status(response);
        const char* resp_body = http_get_body(response);
        if (status >= 200 && status < 300) {
            printf("[+] 消息发送成功 (HTTP %d):\n", status);
            if (strlen(resp_body) > 0) print_json(resp_body, 0);
            return 0;
        } else {
            printf("[-] HTTP %d\n", status);
            if (strlen(resp_body) > 0) print_json(resp_body, 4);
            return -1;
        }

    } else {
        fprintf(stderr, "未知 msg 子命令: %s\n", subcmd);
        return -1;
    }
}

void init_cmd_msg(void)
{
    register_command("msg",
                     "消息操作 (list/get/send)",
                     "msg <list|get|send> ...",
                     cmd_msg);
}
