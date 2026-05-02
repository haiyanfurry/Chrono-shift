/**
 * cmd_config.c — 配置管理命令
 * 对应 debug_cli.c:2311 cmd_config
 */
#include "../devtools_cli.h"

/* ============================================================
 * http_request / http_get_status / http_get_body (由 core/net_http.c 提供)
 * ============================================================ */
extern int http_request(const char* method, const char* path,
                        const char* headers, const char* body,
                        char* response, size_t resp_size);
extern int http_get_status(const char* response);
extern const char* http_get_body(const char* response);

/* ============================================================
 * config 命令
 * ============================================================ */
static int cmd_config(int argc, char** argv)
{
    if (argc < 1) {
        printf("用法:\n");
        printf("  config show                    - 查看客户端配置\n");
        printf("  config set <key> <value>       - 修改配置项\n");
        printf("    可用配置项:\n");
        printf("      host   - 服务器地址 (如 127.0.0.1)\n");
        printf("      port   - 服务器端口 (如 4443)\n");
        printf("      tls    - TLS 开关 (1/0)\n");
        printf("      verbose- 详细模式 (1/0)\n");
        return -1;
    }

    const char* subcmd = argv[0];

    if (strcmp(subcmd, "show") == 0) {
        printf("\n");
        printf("  ╔══════════════════════════════════════════════════════════╗\n");
        printf("  ║     客户端配置 (Config)                                  ║\n");
        printf("  ╚══════════════════════════════════════════════════════════╝\n");
        printf("\n");
        printf("  ┌───────────────────────┬────────────────────────────────┐\n");
        printf("  │ 配置项                │ 当前值                        │\n");
        printf("  ├───────────────────────┼────────────────────────────────┤\n");
        printf("  │ 服务器地址 (host)     │ %-30s │\n", g_config.host);
        printf("  │ 服务器端口 (port)     │ %-30d │\n", g_config.port);
        printf("  │ TLS 启用 (tls)        │ %-30s │\n", g_config.use_tls ? "是 (1)" : "否 (0)");
        printf("  │ 详细模式 (verbose)    │ %-30s │\n", g_config.verbose ? "开 (1)" : "关 (0)");
        printf("  │ 会话状态              │ %-30s │\n", g_config.session_logged_in ? "已登录" : "未登录");
        printf("  │ WebSocket 状态        │ %-30s │\n", g_config.ws_connected ? "已连接" : "未连接");
        printf("  └───────────────────────┴────────────────────────────────┘\n");
        printf("\n");
        printf("  环境变量:\n");
        printf("    CHRONO_HOST = %s\n", getenv("CHRONO_HOST") ? getenv("CHRONO_HOST") : "(未设置)");
        printf("    CHRONO_PORT = %s\n", getenv("CHRONO_PORT") ? getenv("CHRONO_PORT") : "(未设置)");
        printf("    CHRONO_TLS  = %s\n", getenv("CHRONO_TLS") ? getenv("CHRONO_TLS") : "(未设置)");
        return 0;

    } else if (strcmp(subcmd, "set") == 0) {
        if (argc < 3) {
            fprintf(stderr, "用法: config set <key> <value>\n");
            return -1;
        }
        const char* key = argv[1];
        const char* value = argv[2];

        if (strcmp(key, "host") == 0) {
            snprintf(g_config.host, sizeof(g_config.host), "%s", value);
            printf("[+] host 已设为: %s\n", g_config.host);
        } else if (strcmp(key, "port") == 0) {
            int p = atoi(value);
            if (p <= 0 || p > 65535) {
                fprintf(stderr, "[-] 无效端口: %s (1-65535)\n", value);
                return -1;
            }
            g_config.port = p;
            printf("[+] port 已设为: %d\n", g_config.port);
        } else if (strcmp(key, "tls") == 0) {
            g_config.use_tls = (atoi(value) != 0) ? 1 : 0;
            printf("[+] tls 已设为: %s\n", g_config.use_tls ? "启用" : "禁用");
        } else if (strcmp(key, "verbose") == 0) {
            g_config.verbose = (atoi(value) != 0) ? 1 : 0;
            printf("[+] verbose 已设为: %s\n", g_config.verbose ? "开" : "关");
        } else {
            fprintf(stderr, "[-] 未知配置项: %s (可用: host, port, tls, verbose)\n", key);
            return -1;
        }
        return 0;

    } else {
        fprintf(stderr, "未知 config 子命令: %s\n", subcmd);
        return -1;
    }
}

int init_cmd_config(void)
{
    register_command("config",
        "配置管理 (show/set host/port/tls/verbose)",
        "config show | config set <key> <value>",
        cmd_config);
    return 0;
}
