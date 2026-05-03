/**
 * cmd_ping.cpp �?服务器延迟测试命�?(C++23 版本)
 */
#include "../devtools_cli.hpp"

#include <chrono>
#include <cstddef>
#include <cstdlib>
#include "print_compat.h"
#include <string>
#include <thread>

namespace cli = chrono::client::cli;

#ifdef _WIN32
#define msleep(x) std::this_thread::sleep_for(std::chrono::milliseconds(x))
#else
#define msleep(x) std::this_thread::sleep_for(std::chrono::milliseconds(x))
#endif

// ============================================================
// HTTP 底层函数 �?�?net_http.cpp 通过 extern "C" 提供
// ============================================================
extern "C" {
extern int http_request(const char* method, const char* path,
                        const char* headers, const char* body,
                        char* response, size_t resp_size);
extern int http_get_status(const char* response);
extern const char* tls_last_error(void);
}

// ============================================================
// ping 命令 - 服务器延迟测�?
// ============================================================
static int cmd_ping(int argc, char** argv)
{
    int count = 3;
    if (argc >= 1) {
        count = std::atoi(argv[0]);
        if (count < 1) count = 1;
        if (count > 20) {
            cli::println("[-] ping 次数限制�?1-20 之间");
            count = 20;
        }
    }

    cli::println("[*] 开�?ping {}:{} ({} �?...",
                 cli::g_cli_config.host, cli::g_cli_config.port, count);
    cli::println("");

    int succeeded = 0;
    int failed = 0;
    double total_time = 0;
    double min_time = 999999;
    double max_time = 0;

    for (int i = 0; i < count; i++) {
        std::string response(8192, '\0');

        auto start = std::chrono::steady_clock::now();
        int ret = http_request("GET", "/api/health", nullptr, nullptr,
                                response.data(), response.size());
        auto end = std::chrono::steady_clock::now();

        auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(
            end - start).count();
        double elapsed = static_cast<double>(elapsed_us) / 1000.0;

        if (ret == 0) {
            int status = http_get_status(response.c_str());
            succeeded++;
            total_time += elapsed;
            if (elapsed < min_time) min_time = elapsed;
            if (elapsed > max_time) max_time = elapsed;

            cli::println("  [{}/{}] 响应时间: {:.1f} ms  HTTP {}",
                         i + 1, count, elapsed, status);
        } else {
            failed++;
            cli::println("  [{}/{}] 失败: {}",
                         i + 1, count, tls_last_error());
        }

        /* 请求间间�?200ms */
        if (i < count - 1) {
            msleep(200);
        }
    }

    cli::println("");
    cli::println("[*] Ping 统计:");
    cli::println("    发�? {}, 成功: {}, 失败: {}", count, succeeded, failed);
    if (succeeded > 0) {
        cli::println("    平均: {:.1f} ms", total_time / succeeded);
        cli::println("    最�? {:.1f} ms", min_time);
        cli::println("    最�? {:.1f} ms", max_time);
    }

    return (failed == 0) ? 0 : -1;
}

extern "C" int init_cmd_ping(void)
{
    register_command("ping",
        "服务器延迟测�?(默认3�?",
        "ping [count]",
        cmd_ping);
    return 0;
}
