/**
 * cmd_db.c - 数据库调试命令
 * 对应 debug_cli.c:2180 cmd_db
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

extern int cmd_user_list(void); /* 重用 user list */

/** db - 数据库调试命令 */
static int cmd_db(int argc, char** argv)
{
    if (argc < 1) {
        printf("用法:\n");
        printf("  db list users                  - 列出所有用户\n");
        printf("  db list messages               - 列出消息数据\n");
        printf("  db list friends                - 列出好友关系\n");
        printf("  db list templates              - 列出所有模板\n");
        return -1;
    }

    const char* subcmd = argv[0];

    if (strcmp(subcmd, "list") == 0) {
        if (argc < 2) {
            fprintf(stderr, "用法: db list <type> (users/messages/friends/templates)\n");
            return -1;
        }
        const char* type = argv[1];
        char path[512];

        if (strcmp(type, "users") == 0) {
            return cmd_user_list();
        } else if (strcmp(type, "messages") == 0) {
            snprintf(path, sizeof(path), "/api/messages");
        } else if (strcmp(type, "friends") == 0) {
            snprintf(path, sizeof(path), "/api/friendships");
        } else if (strcmp(type, "templates") == 0) {
            snprintf(path, sizeof(path), "/api/templates?limit=100&offset=0");
        } else {
            fprintf(stderr, "未知类型: %s (可用: users, messages, friends, templates)\n", type);
            return -1;
        }

        printf("[*] 查询数据库: %s\n", path);
        char response[BUFFER_SIZE] = {0};
        if (http_request("GET", path, NULL, NULL, response, sizeof(response)) != 0) {
            printf("[-] 请求失败\n"); return -1;
        }
        int status = http_get_status(response);
        const char* body = http_get_body(response);
        if (status >= 200 && status < 300) {
            printf("[+] 数据 (HTTP %d):\n", status);
            if (strlen(body) > 0) print_json(body, 0);
            return 0;
        } else {
            printf("[-] HTTP %d\n", status);
            if (strlen(body) > 0) print_json(body, 4);
            return -1;
        }
    } else {
        fprintf(stderr, "未知 db 子命令: %s\n", subcmd);
        return -1;
    }
}

void init_cmd_db(void)
{
    register_command("db",
                     "数据库调试 (list users/messages/friends/templates)",
                     "db list <type>",
                     cmd_db);
}
