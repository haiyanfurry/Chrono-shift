/**
 * cmd_ping.c — 服务器延迟测试命令
 * 对应 debug_cli.c:1240 cmd_ping
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
extern const char* tls_last_error(void);

/* ============================================================
 * ping 命令 - 服务器延迟测试
 * ============================================================ */
static int cmd_ping(int argc, char** argv)
{
    int count = 3;
    if (argc >= 1) {
        count = atoi(argv[0]);
        if (count < 1) count = 1;
        if (count > 20) {
            printf("[-] ping 次数限制在 1-20 之间\n");
            count = 20;
        }
    }

    printf("[*] 开始 ping %s:%d (%d 次)...\n\n",
           g_config.host, g_config.port, count);

    int succeeded = 0;
    int failed = 0;
    double total_time = 0;
    double min_time = 999999;
    double max_time = 0;

    for (int i = 0; i < count; i++) {
        char response[8192] = {0};

        clock_t start = clock();
        int ret = http_request("GET", "/api/health", NULL, NULL,
                                response, sizeof(response));
        clock_t end = clock();

        double elapsed = ((double)(end - start)) / CLOCKS_PER_SEC * 1000.0;

        if (ret == 0) {
            int status = http_get_status(response);
            succeeded++;
            total_time += elapsed;
            if (elapsed < min_time) min_time = elapsed;
            if (elapsed > max_time) max_time = elapsed;

            printf("  [%d/%d] 响应时间: %.1f ms  HTTP %d\n",
                   i + 1, count, elapsed, status);
        } else {
            failed++;
            printf("  [%d/%d] 失败: %s\n",
                   i + 1, count, tls_last_error());
        }

        /* 请求间间隔 200ms */
        if (i < count - 1) {
            msleep(200);
        }
    }

    printf("\n");
    printf("[*] Ping 统计:\n");
    printf("    发送: %d, 成功: %d, 失败: %d\n", count, succeeded, failed);
    if (succeeded > 0) {
        printf("    平均: %.1f ms\n", total_time / succeeded);
        printf("    最小: %.1f ms\n", min_time);
        printf("    最大: %.1f ms\n", max_time);
    }

    return (failed == 0) ? 0 : -1;
}

int init_cmd_ping(void)
{
    register_command("ping",
        "服务器延迟测试 (默认3次)",
        "ping [count]",
        cmd_ping);
    return 0;
}
