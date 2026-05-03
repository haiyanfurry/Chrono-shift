/**
 * cmd_user.c - 用户管理命令
 * 对应 debug_cli.c:709-887 cmd_user (list/get/create/delete)
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

/** user list - 列出所有用户 */
int cmd_user_list(void)
{
    printf("[*] 获取用户列表...\n");

    char response[BUFFER_SIZE] = {0};
    if (http_request("GET", "/api/users", NULL, NULL,
                      response, sizeof(response)) != 0) {
        printf("[-] 请求失败\n");
        return -1;
    }

    int status = http_get_status(response);
    const char* body = http_get_body(response);

    if (status >= 200 && status < 300) {
        printf("[+] 用户列表 (HTTP %d):\n", status);
        if (strlen(body) > 0) print_json(body, 0);
        return 0;
    } else {
        printf("[-] HTTP %d\n", status);
        if (strlen(body) > 0) print_json(body, 4);
        return -1;
    }
}

/** user get - 获取指定用户信息 */
static int cmd_user_get(const char* user_id)
{
    printf("[*] 获取用户信息: %s\n", user_id);

    char path[512];
    snprintf(path, sizeof(path), "/api/user/profile?id=%s", user_id);

    char response[BUFFER_SIZE] = {0};
    if (http_request("GET", path, NULL, NULL,
                      response, sizeof(response)) != 0) {
        printf("[-] 请求失败\n");
        return -1;
    }

    int status = http_get_status(response);
    const char* body = http_get_body(response);

    if (status >= 200 && status < 300) {
        printf("[+] 用户信息 (HTTP %d):\n", status);
        if (strlen(body) > 0) print_json(body, 0);
        return 0;
    } else {
        printf("[-] HTTP %d\n", status);
        if (strlen(body) > 0) print_json(body, 4);
        return -1;
    }
}

/** user create - 创建用户 */
static int cmd_user_create(const char* username, const char* password, const char* nickname)
{
    printf("[*] 创建用户: username=%s", username);
    if (nickname) printf(", nickname=%s", nickname);
    printf("\n");

    char body[1024];
    if (nickname) {
        snprintf(body, sizeof(body),
            "{\"username\":\"%s\",\"password\":\"%s\",\"nickname\":\"%s\"}",
            username, password, nickname);
    } else {
        snprintf(body, sizeof(body),
            "{\"username\":\"%s\",\"password\":\"%s\"}",
            username, password);
    }

    char response[BUFFER_SIZE] = {0};
    if (http_request("POST", "/api/user/register", body, "application/json",
                      response, sizeof(response)) != 0) {
        printf("[-] 请求失败\n");
        return -1;
    }

    int status = http_get_status(response);
    const char* resp_body = http_get_body(response);

    if (status >= 200 && status < 300) {
        printf("[+] 用户创建成功 (HTTP %d):\n", status);
        if (strlen(resp_body) > 0) print_json(resp_body, 0);
        return 0;
    } else {
        printf("[-] HTTP %d\n", status);
        if (strlen(resp_body) > 0) print_json(resp_body, 4);
        return -1;
    }
}

/** user delete - 删除用户 */
static int cmd_user_delete(const char* user_id)
{
    printf("[*] 删除用户: %s\n", user_id);

    char path[512];
    snprintf(path, sizeof(path), "/api/user?id=%s", user_id);

    char response[BUFFER_SIZE] = {0};
    if (http_request("DELETE", path, NULL, NULL,
                      response, sizeof(response)) != 0) {
        printf("[-] 请求失败\n");
        return -1;
    }

    int status = http_get_status(response);
    const char* body = http_get_body(response);

    if (status >= 200 && status < 300) {
        printf("[+] 用户删除成功 (HTTP %d)\n", status);
        if (strlen(body) > 0) print_json(body, 0);
        return 0;
    } else {
        printf("[-] HTTP %d\n", status);
        if (strlen(body) > 0) print_json(body, 4);
        return -1;
    }
}

/** user - 用户管理入口 */
static int cmd_user(int argc, char** argv)
{
    if (argc < 1) {
        printf("用法:\n");
        printf("  user list                     - 列出所有用户\n");
        printf("  user get <id>                 - 获取用户信息\n");
        printf("  user create <username> <pass> [nickname] - 创建用户\n");
        printf("  user delete <id>              - 删除用户\n");
        return -1;
    }

    const char* subcmd = argv[0];

    if (strcmp(subcmd, "list") == 0) {
        return cmd_user_list();
    } else if (strcmp(subcmd, "get") == 0) {
        if (argc < 2) {
            fprintf(stderr, "用法: user get <user_id>\n");
            return -1;
        }
        return cmd_user_get(argv[1]);
    } else if (strcmp(subcmd, "create") == 0) {
        if (argc < 3) {
            fprintf(stderr, "用法: user create <username> <password> [nickname]\n");
            return -1;
        }
        return cmd_user_create(argv[1], argv[2], argc >= 4 ? argv[3] : NULL);
    } else if (strcmp(subcmd, "delete") == 0) {
        if (argc < 2) {
            fprintf(stderr, "用法: user delete <user_id>\n");
            return -1;
        }
        return cmd_user_delete(argv[1]);
    } else {
        fprintf(stderr, "未知 user 子命令: %s\n", subcmd);
        fprintf(stderr, "可用命令: list, get, create, delete\n");
        return -1;
    }
}

void init_cmd_user(void)
{
    register_command("user",
                     "用户管理 (list/get/create/delete)",
                     "user <list|get|create|delete> ...",
                     cmd_user);
}
