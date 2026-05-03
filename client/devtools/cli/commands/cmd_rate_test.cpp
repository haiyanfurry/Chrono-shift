/**
 * cmd_rate_test.cpp — 速率测试命令
 * 对应 debug_cli.c:1396 cmd_rate_test
 *
 * C++23 转换: std::println, std::chrono, namespace cli
 */
#include "../devtools_cli.hpp"

#include <chrono>    // std::chrono::steady_clock
#include <print>     // std::println
#include <cstdlib>   // std::atoi

namespace cli = chrono::client::cli;

/* ============================================================
 * http_request / tls_last_error
 * ============================================================ */
extern "C" {
extern int http_request(const char* method, const char* path,
                        const char* headers, const char* body,
                        char* response, size_t resp_size);
extern int http_get_status(const char* response);
extern const char* tls_last_error(void);
}

/* ============================================================
 * rate-test 命令 - 速率/吞吐率测试
 * ============================================================ */
static int cmd_rate_test(int argc, char** argv)
{
    int num_requests = 5;
    if (argc >= 1) {
        num_requests = std::atoi(argv[0]);
        if (num_requests < 1) num_requests = 1;
        if (num_requests > 50) {
            std::println("[-] 并发数限制在 1-50");
            num_requests = 50;
        }
    }

    std::println("[*] 开始速率测试: {} 个并发请求 -> {}:{}",
                 num_requests, cli::g_cli_config.host, cli::g_cli_config.port);
    std::println("    测试端点: /api/health");
    std::println("");

    int success = 0;
    int failure = 0;
    double total_time = 0;

    auto test_start = std::chrono::steady_clock::now();

    for (int i = 0; i < num_requests; i++) {
        char response[8192]{};

        auto req_start = std::chrono::steady_clock::now();
        int ret = http_request("GET", "/api/health", nullptr, nullptr,
                                response, sizeof(response));
        auto req_end = std::chrono::steady_clock::now();

        auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(req_end - req_start).count();
        double elapsed_ms = elapsed_us / 1000.0;

        if (ret == 0) {
            int status = http_get_status(response);
            success++;
            total_time += elapsed_ms;
            std::println("  [{:3}/{}] ✓ HTTP {}  {:.1f} ms",
                         i + 1, num_requests, status, elapsed_ms);
        } else {
            failure++;
            std::println("  [{:3}/{}] ✗ 失败: {}",
                         i + 1, num_requests, tls_last_error());
        }
    }

    auto test_end = std::chrono::steady_clock::now();
    auto total_elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(test_end - test_start).count();
    double total_elapsed_ms = total_elapsed_us / 1000.0;

    std::println("");
    std::println("[*] 速率测试结果:");
    std::println("    总请求: {}", num_requests);
    std::println("    成功:   {}", success);
    std::println("    失败:   {}", failure);
    std::println("    总耗时: {:.0f} ms", total_elapsed_ms);
    if (success > 0) {
        std::println("    平均响应: {:.1f} ms", total_time / success);
        std::println("    吞吐率:   {:.1f} req/s",
                     success / (total_elapsed_ms / 1000.0));
    }

    return (failure == 0) ? 0 : -1;
}

extern "C" int init_cmd_rate_test(void)
{
    register_command("rate-test",
        "速率/吞吐率测试",
        "rate-test [n]",
        cmd_rate_test);
    return 0;
}
