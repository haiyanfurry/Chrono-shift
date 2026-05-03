/**
 * cmd_watch.cpp — 实时监控服务器状态命令
 * 对应 debug_cli.c:1305 cmd_watch
 *
 * C++23 转换: std::println, std::chrono, std::this_thread::sleep_for, namespace cli
 */
#include "../devtools_cli.hpp"

#include <chrono>    // std::chrono::steady_clock, std::chrono::milliseconds
#include <print>     // std::println
#include <thread>    // std::this_thread::sleep_for
#include <string>    // std::string

namespace cli = chrono::client::cli;

/* ============================================================
 * http_request / tls_last_error
 * ============================================================ */
extern "C" {
extern int http_request(const char* method, const char* path,
                        const char* headers, const char* body,
                        char* response, size_t resp_size);
extern int http_get_status(const char* response);
extern const char* http_get_body(const char* response);
extern const char* tls_last_error(void);
}

/* ============================================================
 * watch 命令 - 实时监控服务器状态
 * ============================================================ */
static int cmd_watch(int argc, char** argv)
{
    int interval = 2;
    if (argc >= 1) {
        interval = std::atoi(argv[0]);
        if (interval < 1) interval = 1;
        if (interval > 30) {
            std::println("[-] watch 间隔限制在 1-30 秒");
            interval = 30;
        }
    }

    int max_rounds = 10;
    if (argc >= 2) {
        max_rounds = std::atoi(argv[1]);
        if (max_rounds < 1) max_rounds = 1;
        if (max_rounds > 100) {
            std::println("[-] watch 轮次限制在 1-100");
            max_rounds = 100;
        }
    }

    std::println("[*] 开始监控 {}:{} (间隔 {}s, {} 轮)...",
                 cli::g_cli_config.host, cli::g_cli_config.port,
                 interval, max_rounds);
    std::println("    按 Ctrl+C 终止...");
    std::println("");

    int prev_status = -1;
    int unstable_count = 0;

    for (int round = 1; round <= max_rounds; round++) {
        char response[8192]{};

        auto start = std::chrono::steady_clock::now();
        int ret = http_request("GET", "/api/health", nullptr, nullptr,
                                response, sizeof(response));
        auto end = std::chrono::steady_clock::now();

        auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        double elapsed_ms = elapsed_us / 1000.0;

        auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        char time_str[32];
        std::strftime(time_str, sizeof(time_str), "%H:%M:%S", std::localtime(&now));

        if (ret == 0) {
            int status = http_get_status(response);
            const char* body = http_get_body(response);

            if (status != prev_status) {
                if (prev_status != -1) {
                    std::println("  [{}] ⚠ 状态变化: {} -> {}",
                                 time_str, prev_status, status);
                }
                prev_status = status;
            }

            const char* status_flag = (status >= 200 && status < 300) ? "✓" : "⚠";
            std::println("  [{}] {} HTTP {}  {:.1f} ms",
                         time_str, status_flag, status, elapsed_ms);

            if (status >= 400) {
                unstable_count++;
                std::println("        响应: {}", body ? body : "(空)");
            } else {
                unstable_count = 0;
            }

        } else {
            std::println("  [{}] ✗ 连接失败: {}", time_str, tls_last_error());
            prev_status = -1;
            unstable_count++;
        }

        if (unstable_count >= 3) {
            std::println("");
            std::println("[-] 警告: 连续 {} 次异常, 服务器可能不稳定!", unstable_count);
        }

        std::println("");

        if (round < max_rounds) {
            for (int s = 0; s < interval; s++) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
    }

    std::println("[*] 监控完成 ({} 轮)", max_rounds);
    return (unstable_count == 0) ? 0 : -1;
}

extern "C" int init_cmd_watch(void)
{
    register_command("watch",
        "实时监控服务器状态",
        "watch [interval] [rounds]",
        cmd_watch);
    return 0;
}
