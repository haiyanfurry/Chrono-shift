/**
 * cmd_rate_test.c — 速率测试命令
 * 对应 debug_cli.c:1396 cmd_rate_test
 */
#include "../devtools_cli.h"
#include <time.h>

/* ============================================================
 * http_request / tls_last_error
 * ============================================================ */
extern int http_request(const char* method, const char* path,
                        const char* headers, const char* body,
                        char* response, size_t resp_size);
extern int http_get_status(const char* response);
extern const char* tls_last_error(void);

/* ============================================================
 * rate-test 命令 - 速率/吞吐率测试
 * ============================================================ */
static int cmd_rate_test(int argc, char** argv)
{
    int num_requests = 5;
    if (argc >= 1) {
        num_requests = atoi(argv[0]);
        if (num_requests < 1) num_requests = 1;
        if (num_requests > 50) {
            printf("[-] 并发数限制在 1-50\n");
            num_requests = 50;
        }
    }

    printf("[*] 开始速率测试: %d 个并发请求 -> %s:%d\n",
           num_requests, g_config.host, g_config.port);
    printf("    测试端点: /api/health\n\n");

    int success = 0;
    int failure = 0;
    double total_time = 0;

    clock_t test_start = clock();

    for (int i = 0; i < num_requests; i++) {
        char response[8192] = {0};

        clock_t req_start = clock();
        int ret = http_request("GET", "/api/health", NULL, NULL,
                                response, sizeof(response));
        clock_t req_end = clock();

        double elapsed = ((double)(req_end - req_start)) / CLOCKS_PER_SEC * 1000.0;

        if (ret == 0) {
            int status = http_get_status(response);
            success++;
            total_time += elapsed;
            printf("  [%3d/%d] ✓ HTTP %d  %.1f ms\n",
                   i + 1, num_requests, status, elapsed);
        } else {
            failure++;
            printf("  [%3d/%d] ✗ 失败: %s\n",
                   i + 1, num_requests, tls_last_error());
        }
    }

    clock_t test_end = clock();
    double total_elapsed = ((double)(test_end - test_start)) / CLOCKS_PER_SEC * 1000.0;

    printf("\n");
    printf("[*] 速率测试结果:\n");
    printf("    总请求: %d\n", num_requests);
    printf("    成功:   %d\n", success);
    printf("    失败:   %d\n", failure);
    printf("    总耗时: %.0f ms\n", total_elapsed);
    if (success > 0) {
        printf("    平均响应: %.1f ms\n", total_time / success);
        printf("    吞吐率:   %.1f req/s\n",
               success / (total_elapsed / 1000.0));
    }

    return (failure == 0) ? 0 : -1;
}

int init_cmd_rate_test(void)
{
    register_command("rate-test",
        "速率/吞吐率测试",
        "rate-test [n]",
        cmd_rate_test);
    return 0;
}
