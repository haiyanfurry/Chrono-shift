/**
 * cmd_watch.c — 实时监控服务器状态命令
 * 对应 debug_cli.c:1305 cmd_watch
 */
#include "../devtools_cli.h"
#include <time.h>

#ifdef _WIN32
    #include <windows.h>
    #define msleep(x) Sleep(x)
#else
    #include <unistd.h>
    #define msleep(x) usleep((x) * 1000)
#endif

/* ============================================================
 * http_request / tls_last_error
 * ============================================================ */
extern int http_request(const char* method, const char* path,
                        const char* headers, const char* body,
                        char* response, size_t resp_size);
extern int http_get_status(const char* response);
extern const char* http_get_body(const char* response);
extern const char* tls_last_error(void);

/* ============================================================
 * watch 命令 - 实时监控服务器状态
 * ============================================================ */
static int cmd_watch(int argc, char** argv)
{
    int interval = 2;
    if (argc >= 1) {
        interval = atoi(argv[0]);
        if (interval < 1) interval = 1;
        if (interval > 30) {
            printf("[-] watch 间隔限制在 1-30 秒\n");
            interval = 30;
        }
    }

    int max_rounds = 10;
    if (argc >= 2) {
        max_rounds = atoi(argv[1]);
        if (max_rounds < 1) max_rounds = 1;
        if (max_rounds > 100) {
            printf("[-] watch 轮次限制在 1-100\n");
            max_rounds = 100;
        }
    }

    printf("[*] 开始监控 %s:%d (间隔 %ds, %d 轮)...\n",
           g_config.host, g_config.port, interval, max_rounds);
    printf("    按 Ctrl+C 终止...\n\n");

    int prev_status = -1;
    int unstable_count = 0;

    for (int round = 1; round <= max_rounds; round++) {
        char response[8192] = {0};

        clock_t start = clock();
        int ret = http_request("GET", "/api/health", NULL, NULL,
                                response, sizeof(response));
        clock_t end = clock();

        double elapsed = ((double)(end - start)) / CLOCKS_PER_SEC * 1000.0;

        time_t now = time(NULL);
        char time_str[32];
        strftime(time_str, sizeof(time_str), "%H:%M:%S", localtime(&now));

        if (ret == 0) {
            int status = http_get_status(response);
            const char* body = http_get_body(response);

            if (status != prev_status) {
                if (prev_status != -1) {
                    printf("  [%s] ⚠ 状态变化: %d -> %d\n",
                           time_str, prev_status, status);
                }
                prev_status = status;
            }

            const char* status_flag = (status >= 200 && status < 300) ? "✓" : "⚠";
            printf("  [%s] %s HTTP %d  %.1f ms\n",
                   time_str, status_flag, status, elapsed);

            if (status >= 400) {
                unstable_count++;
                printf("        响应: %s\n", body ? body : "(空)");
            } else {
                unstable_count = 0;
            }

        } else {
            printf("  [%s] ✗ 连接失败: %s\n",
                   time_str, tls_last_error());
            prev_status = -1;
            unstable_count++;
        }

        if (unstable_count >= 3) {
            printf("\n[-] 警告: 连续 %d 次异常, 服务器可能不稳定!\n", unstable_count);
        }

        printf("\n");

        if (round < max_rounds) {
            for (int s = 0; s < interval; s++) {
                msleep(1000);
            }
        }
    }

    printf("[*] 监控完成 (%d 轮)\n", max_rounds);
    return (unstable_count == 0) ? 0 : -1;
}

int init_cmd_watch(void)
{
    register_command("watch",
        "实时监控服务器状态",
        "watch [interval] [rounds]",
        cmd_watch);
    return 0;
}
