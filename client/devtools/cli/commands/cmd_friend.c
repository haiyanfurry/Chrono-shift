/**
 * cmd_friend.c - 好友操作命令
 * 对应 debug_cli.c:2097 cmd_friend
 */
#include "../devtools_cli.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

/* 简易 JSON 字符串转义 */
static void escape_json_str(char* dst, const char* src, size_t dst_size)
{
    size_t j = 0;
    for (size_t i = 0; src[i] && j < dst_size - 1; i++) {
        if (src[i] == '"' || src[i] == '\\') {
            if (j < dst_size - 2) { dst[j++] = '\\'; dst[j++] = src[i]; }
        } else {
            dst[j++] = src[i];
        }
    }
    dst[j] = '\0';
}

extern int http_request(const char* method, const char* path,
                        const char* body, const char* content_type,
                        char* response, size_t resp_size);
extern int http_get_status(const char* response);
extern const char* http_get_body(const char* response);

#define BUFFER_SIZE 65536

/** friend - 好友操作命令 */
static int cmd_friend(int argc, char** argv)
{
    if (argc < 1) {
        printf("用法:\n");
        printf("  friend list <user_id>         - 列出用户好友列表\n");
        printf("  friend add <uid1> <uid2>      - 添加好友关系\n");
        return -1;
    }

    const char* subcmd = argv[0];

    if (strcmp(subcmd, "list") == 0) {
        if (argc < 2) { fprintf(stderr, "用法: friend list <user_id>\n"); return -1; }
        const char* uid = argv[1];
        char path[512];
        snprintf(path, sizeof(path), "/api/friends?id=%s", uid);
        printf("[*] 获取好友列表: user_id=%s\n", uid);

        char response[BUFFER_SIZE] = {0};
        if (http_request("GET", path, NULL, NULL, response, sizeof(response)) != 0) {
            printf("[-] 请求失败\n"); return -1;
        }
        int status = http_get_status(response);
        const char* body = http_get_body(response);
        if (status >= 200 && status < 300) {
            printf("[+] 好友列表 (HTTP %d):\n", status);
            if (strlen(body) > 0) print_json(body, 0);
            return 0;
        } else {
            printf("[-] HTTP %d\n", status);
            if (strlen(body) > 0) print_json(body, 4);
            return -1;
        }

    } else if (strcmp(subcmd, "add") == 0) {
        if (argc < 3) { fprintf(stderr, "用法: friend add <user_id1> <user_id2>\n"); return -1; }
        char safe_id1[128], safe_id2[128];
        escape_json_str(safe_id1, argv[1], sizeof(safe_id1));
        escape_json_str(safe_id2, argv[2], sizeof(safe_id2));
        char body[512];
        snprintf(body, sizeof(body),
            "{\"user_id1\":\"%s\",\"user_id2\":\"%s\"}", safe_id1, safe_id2);
        printf("[*] 添加好友: %s <-> %s\n", safe_id1, safe_id2);

        char response[BUFFER_SIZE] = {0};
        if (http_request("POST", "/api/friends/add", body, "application/json",
                          response, sizeof(response)) != 0) {
            printf("[-] 请求失败\n"); return -1;
        }
        int status = http_get_status(response);
        const char* resp_body = http_get_body(response);
        if (status >= 200 && status < 300) {
            printf("[+] 好友添加成功 (HTTP %d):\n", status);
            if (strlen(resp_body) > 0) print_json(resp_body, 0);
            return 0;
        } else {
            printf("[-] HTTP %d\n", status);
            if (strlen(resp_body) > 0) print_json(resp_body, 4);
            return -1;
        }

    } else {
        fprintf(stderr, "未知 friend 子命令: %s\n", subcmd);
        return -1;
    }
}

void init_cmd_friend(void)
{
    register_command("friend",
                     "好友操作 (list/add)",
                     "friend <list|add> ...",
                     cmd_friend);
}
