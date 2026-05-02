/**
 * cmd_storage.c — 安全存储命令
 * 对应 debug_cli.c:2388 cmd_storage
 */
#include "../devtools_cli.h"

/* ============================================================
 * storage 命令 - 本地安全存储查看
 * ============================================================ */
static int cmd_storage(int argc, char** argv)
{
    if (argc < 1) {
        printf("用法:\n");
        printf("  storage list                   - 列出本地安全存储内容\n");
        printf("  storage get <key>              - 读取安全存储条目\n");
        return -1;
    }

    /* 初始化存储路径 */
    if (g_config.storage_path[0] == 0) {
#ifdef _WIN32
        const char* appdata = getenv("APPDATA");
        snprintf(g_config.storage_path, sizeof(g_config.storage_path),
                 "%s/Chrono-shift/secure", appdata ? appdata : ".");
#else
        const char* home = getenv("HOME");
        snprintf(g_config.storage_path, sizeof(g_config.storage_path),
                 "%s/.chrono-shift/secure", home ? home : ".");
#endif
    }

    const char* subcmd = argv[0];

    if (strcmp(subcmd, "list") == 0) {
        printf("\n");
        printf("  ╔══════════════════════════════════════════════════════════╗\n");
        printf("  ║     本地安全存储 (Secure Storage)                        ║\n");
        printf("  ╚══════════════════════════════════════════════════════════╝\n");
        printf("\n");
        printf("  存储路径: %s\n", g_config.storage_path);
        printf("\n");
        printf("  存储内容 (模拟):\n");
        printf("  ┌──────────┬────────────────────────────────────────────┐\n");
        printf("  │ 键名     │ 值                                         │\n");
        printf("  ├──────────┼────────────────────────────────────────────┤\n");
        printf("  │ token    │ %-42s │\n",
               g_config.session_logged_in ? g_config.session_token : "(空)");
        printf("  │ user_id  │ %-42s │\n",
               g_config.session_logged_in ? "1" : "(空)");
        printf("  │ device   │ chrono-cli (当前工具)                      │\n");
        printf("  └──────────┴────────────────────────────────────────────┘\n");
        printf("\n");
        printf("  说明: 生产环境中使用 AES-256-GCM 加密存储\n");
        printf("  实现:  client/security/src/secure_storage.rs (Rust FFI)\n");
        printf("  路径:  %s/*.chrono_*\n", g_config.storage_path);
        return 0;

    } else if (strcmp(subcmd, "get") == 0) {
        if (argc < 2) {
            fprintf(stderr, "用法: storage get <key>\n");
            return -1;
        }
        const char* key = argv[1];

        printf("[*] 读取安全存储: key=%s\n", key);
        if (strcmp(key, "token") == 0) {
            if (g_config.session_logged_in) {
                printf("[+] token = %s\n", g_config.session_token);
            } else {
                printf("[*] token = (未设置, 请先 session login)\n");
            }
        } else {
            printf("[-] 键 '%s' 不存在于本地存储\n", key);
            printf("[*] 可用键: token, user_id, device\n");
            return -1;
        }
        return 0;

    } else {
        fprintf(stderr, "未知 storage 子命令: %s\n", subcmd);
        return -1;
    }
}

int init_cmd_storage(void)
{
    register_command("storage",
        "安全存储管理 (list/get)",
        "storage list | storage get <key>",
        cmd_storage);
    return 0;
}
