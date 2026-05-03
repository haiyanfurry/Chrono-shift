/**
 * cmd_session.c - 会话管理命令
 * 对应 debug_cli.c:2246 cmd_session
 */
#include "../devtools_cli.h"
#include <stdio.h>
#include <string.h>

/** session - 会话管理命令 */
static int cmd_session(int argc, char** argv)
{
    if (argc < 1) {
        printf("用法:\n");
        printf("  session show                    - 查看当前会话状态\n");
        printf("  session login <host> <token>    - 登录并保存会话\n");
        printf("  session logout                  - 清除会话\n");
        return -1;
    }

    const char* subcmd = argv[0];

    if (strcmp(subcmd, "show") == 0) {
        printf("\n");
        printf("  ╔══════════════════════════════════════════════════════════╗\n");
        printf("  ║     会话状态 (Session)                                   ║\n");
        printf("  ╚══════════════════════════════════════════════════════════╝\n");
        printf("\n");
        printf("  登录状态: %s\n", g_config.session_logged_in ? "已登录 ✓" : "未登录 ✗");
        if (g_config.session_logged_in) {
            printf("  服务器:   %s\n", g_config.session_host);
            /* 令牌脱敏显示: 仅显示前8字符 */
            size_t tok_len = strlen(g_config.session_token);
            printf("  令牌:     %.8s...\n", g_config.session_token);
            printf("  令牌长度: %zu 字符\n", tok_len);
        }
        printf("\n");
        printf("  提示: session login <host> <token> 来登录\n");
        return 0;

    } else if (strcmp(subcmd, "login") == 0) {
        if (argc < 3) { fprintf(stderr, "用法: session login <host> <token>\n"); return -1; }
        const char* host = argv[1];
        const char* token = argv[2];

        snprintf(g_config.session_host, sizeof(g_config.session_host), "%s", host);
        snprintf(g_config.session_token, sizeof(g_config.session_token), "%s", token);
        g_config.session_logged_in = 1;
        snprintf(g_config.host, sizeof(g_config.host), "%s", host);

        printf("[+] 会话已保存: host=%s, token=%.32s...\n", host, token);
        printf("[*] 全局目标已同步为: %s:%d\n", g_config.host, g_config.port);
        return 0;

    } else if (strcmp(subcmd, "logout") == 0) {
        if (!g_config.session_logged_in) { printf("[*] 当前没有活跃会话\n"); return 0; }
        memset(g_config.session_token, 0, sizeof(g_config.session_token));
        memset(g_config.session_host, 0, sizeof(g_config.session_host));
        g_config.session_logged_in = 0;
        printf("[+] 会话已清除\n");
        return 0;

    } else {
        fprintf(stderr, "未知 session 子命令: %s\n", subcmd);
        return -1;
    }
}

void init_cmd_session(void)
{
    register_command("session",
                     "会话管理 (show/login/logout)",
                     "session <show|login|logout> ...",
                     cmd_session);
}
