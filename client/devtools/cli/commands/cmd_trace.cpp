/**
 * cmd_trace.cpp — 请求追踪命令
 * 对应 debug_cli.c:1175 cmd_trace
 *
 * C++23 转换: std::println, std::string_view, namespace cli
 */
#include "../devtools_cli.hpp"

#include <print>     // std::println
#include <string>    // std::string
#include <string_view> // std::string_view
#include <cstring>   // std::strlen, std::strncmp

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

constexpr size_t BUFFER_SIZE = 8192;

/* ============================================================
 * trace 命令 - 追踪请求路径
 * ============================================================ */
static int cmd_trace(int argc, char** argv)
{
    if (argc < 1) {
        std::println(stderr, "用法: trace <path>");
        std::println(stderr, "  发送 TRACE 请求追踪请求经过的路径");
        std::println(stderr, "  示例: trace /api/health");
        return -1;
    }

    const char* path = argv[0];
    std::println("[*] 追踪请求路径: {}", path);
    std::println("    方法: GET -> {}:{}{}", cli::g_cli_config.host, cli::g_cli_config.port, path);
    std::println("");

    /* 追踪过程的模拟步骤 */
    std::println("  [1/5] DNS 解析: {} -> {}:{}",
                 cli::g_cli_config.host, cli::g_cli_config.host, cli::g_cli_config.port);
    std::println("  [2/5] TCP 连接: 建立连接...");

    /* 实际发送请求测试路径 */
    char response[BUFFER_SIZE]{};
    int ret = http_request("GET", path, nullptr, nullptr,
                            response, sizeof(response));

    if (ret == 0) {
        int status = http_get_status(response);
        const char* body = http_get_body(response);

        std::println("  [3/5] TLS 握手: 完成 (HTTPS)");
        std::println("  [4/5] HTTP 请求: {} {} -> HTTP {}", "GET", path, status);
        std::println("  [5/5] 响应处理: 完成");
        std::println("");

        /* 路由匹配分析 */
        std::println("[*] 路由分析:");
        if (std::strncmp(path, "/api/health", 11) == 0)
            std::println("     => health_handler (健康检查)");
        else if (std::strncmp(path, "/api/user", 9) == 0)
            std::println("     => user_handler (用户管理)");
        else if (std::strncmp(path, "/api/message", 12) == 0)
            std::println("     => message_handler (消息处理)");
        else if (std::strncmp(path, "/api/community", 14) == 0)
            std::println("     => community_handler (社区管理)");
        else if (std::strncmp(path, "/api/files", 10) == 0)
            std::println("     => file_handler (文件处理)");
        else if (std::strncmp(path, "/api/ws", 7) == 0)
            std::println("     => websocket_handler (WebSocket)");
        else
            std::println("     => 未知路由或静态文件");

        if (std::strlen(body) > 0) {
            std::println("");
            std::println("[*] 响应数据:");
            print_json(body, 4);
        }

        return (status >= 200 && status < 300) ? 0 : -1;
    }

    std::println("  [3/5] TLS 握手: 失败 - {}", tls_last_error());
    std::println("  [4/5] HTTP 请求: 未发送");
    std::println("  [5/5] 响应处理: 无响应");
    return -1;
}

extern "C" int init_cmd_trace(void)
{
    register_command("trace",
        "追踪请求路径",
        "trace <path>",
        cmd_trace);
    return 0;
}
