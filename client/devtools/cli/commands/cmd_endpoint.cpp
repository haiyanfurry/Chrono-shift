/**
 * cmd_endpoint.cpp �?API 端点测试命令 (C++23 版本)
 */
#include "../devtools_cli.hpp"

#include <cstddef>
#include <cstdio>
#include "print_compat.h
#include <string>
#include <string_view>

// ============================================================
// HTTP 底层函数 �?�?net_http.cpp 通过 extern "C" 提供
// ============================================================
extern "C" {
extern int http_request(const char* method, const char* path,
                        const char* body, const char* content_type,
                        char* response, size_t resp_size);
extern int http_get_status(const char* response);
extern const char* http_get_body(const char* response);
}

namespace cli = chrono::client::cli;

constexpr std::size_t BUFFER_SIZE = 65536;

/** endpoint - 测试 API 端点 */
static int cmd_endpoint(int argc, char** argv)
{
    if (argc < 1) {
        cli::println(stderr, "用法: endpoint <path> [method] [body]");
        cli::println(stderr, "  path   - API 路径, �?/api/user/profile?id=1");
        cli::println(stderr, "  method - HTTP 方法 (GET/POST/PUT/DELETE, 默认 GET)");
        cli::println(stderr, "  body   - POST/PUT 请求�?(JSON 字符�?");
        return -1;
    }

    const char* path   = argv[0];
    const char* method = (argc >= 2) ? argv[1] : "GET";
    const char* body   = (argc >= 3) ? argv[2] : nullptr;

    cli::println("[*] {} {}{}{}", method, path,
                 body ? " body: " : "", body ? body : "");

    std::string response(BUFFER_SIZE, '\0');
    if (http_request(method, path, body, "application/json",
                     response.data(), response.size()) != 0) {
        cli::println("[-] 请求失败");
        return -1;
    }

    int status = http_get_status(response.c_str());
    std::string_view resp_body = http_get_body(response.c_str());

    cli::println("[*] HTTP {}", status);
    if (!resp_body.empty()) {
        cli::print_json(resp_body, 0);
    }
    return (status >= 200 && status < 300) ? 0 : -1;
}

extern "C" void init_cmd_endpoint(void)
{
    register_command("endpoint",
                     "测试 API 端点",
                     "endpoint <path> [method] [body]",
                     cmd_endpoint);
}
