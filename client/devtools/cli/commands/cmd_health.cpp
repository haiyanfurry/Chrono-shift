/**
 * cmd_health.cpp — 健康检查命令 (C++23 版本)
 *
 * 使用 C++23 特性: std::println, std::string, std::string_view
 * extern "C" 链接兼容层供 init_commands.cpp 调用。
 */
#include "../devtools_cli.hpp"

#include <cstddef>
#include "print_compat.h"
#include <string>
#include <string_view>

// ============================================================
// HTTP 底层函数 — 由 net_http.cpp 通过 extern "C" 提供
// ============================================================
extern "C" {
extern int http_request(const char* method, const char* path,
                        const char* body, const char* content_type,
                        char* response, size_t resp_size);
extern int http_get_status(const char* response);
extern const char* http_get_body(const char* response);
}

namespace cli = chrono::client::cli;

// ============================================================
// 常量
// ============================================================
constexpr std::size_t BUFFER_SIZE = 65536;

// ============================================================
// cmd_health — 检查服务器健康状态
// ============================================================
static int cmd_health(int /*argc*/, char** /*argv*/)
{
    cli::println("[*] 检查服务器健康状态: {}:{}",
                 cli::g_cli_config.host,
                 cli::g_cli_config.port);

    std::string response(BUFFER_SIZE, '\0');
    if (http_request("GET", "/api/health", nullptr, nullptr,
                     response.data(), response.size()) != 0) {
        cli::println("[-] 服务器未响应");
        return -1;
    }

    int status = http_get_status(response.c_str());
    std::string_view body = http_get_body(response.c_str());

    cli::println("[*] HTTP {}", status);

    if (status >= 200 && status < 300) {
        cli::println("[+] 服务器运行正常");
        if (!body.empty()) {
            cli::print("    响应: ");
            cli::print_json(body);
        }
        return 0;
    } else if (status >= 400) {
        cli::println("[-] 服务器返回错误");
        if (!body.empty()) {
            cli::print("    响应: ");
            cli::print_json(body);
        }
        return -1;
    } else {
        cli::println("[?] 未知状态码");
        if (!body.empty()) {
            cli::println("    响应: {}", body);
        }
        return -1;
    }
}

// ============================================================
// init_cmd_health — 注册 health 命令 (extern "C" 供 init_commands.cpp 调用)
// ============================================================
extern "C" void init_cmd_health(void)
{
    register_command("health",
                     "检查服务器健康状态",
                     "health",
                     cmd_health);
}
