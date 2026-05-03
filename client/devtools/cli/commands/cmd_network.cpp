/**
 * cmd_network.cpp — 网络诊断命令
 * 对应 debug_cli.c:2635 cmd_network
 *
 * C++23 转换: std::println, std::chrono, std::string, namespace cli
 *
 * 完整的 DNS/TCP/TLS/HTTP 四层连通性诊断
 */
#include "../devtools_cli.hpp"

#include <chrono>    // std::chrono::steady_clock
#include <print>     // std::println
#include <string>    // std::string
#include <string_view> // std::string_view
#include <cstdlib>   // std::atoi
#include <cstring>   // std::memcpy, std::strtok

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#define CLOSE_SOCKET(x) closesocket(x)
#define ISVALIDSOCKET(s) ((s) != INVALID_SOCKET)
using socket_t = SOCKET;
#else
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#define CLOSE_SOCKET(x) close(x)
#define ISVALIDSOCKET(s) ((s) >= 0)
using socket_t = int;
#endif

namespace cli = chrono::client::cli;

/* ============================================================
 * tls_client / http_request 函数
 * ============================================================ */
extern "C" {
extern int tls_client_init(const char* cert_dir);
extern int tls_client_connect(void** ssl, const char* host, unsigned short port);
extern void tls_close(void* ssl);
extern const char* tls_last_error(void);
extern void tls_get_info(void* ssl, char* buf, size_t buf_size);
extern int http_request(const char* method, const char* path,
                        const char* headers, const char* body,
                        char* response, size_t resp_size);
extern int http_get_status(const char* response);
}

/* ============================================================
 * network 命令 - 网络连通性测试
 * ============================================================ */
static int cmd_network(int argc, char** argv)
{
    if (argc < 1) {
        std::println("用法:");
        std::println("  network test <host> <port>     - 网络连通性测试");
        std::println("    测试目标主机的 TCP 连接和 TLS 握手");
        return -1;
    }

    const std::string_view subcmd = argv[0];

    if (subcmd == "test") {
        if (argc < 3) {
            std::println(stderr, "用法: network test <host> <port>");
            std::println(stderr, "示例: network test 127.0.0.1 4443");
            return -1;
        }

        const char* test_host = argv[1];
        int test_port = std::atoi(argv[2]);
        if (test_port <= 0 || test_port > 65535) {
            std::println(stderr, "[-] 无效端口: {}", test_port);
            return -1;
        }

        std::println("");
        std::println("  ╔══════════════════════════════════════════════════════════╗");
        std::println("  ║     网络连通性测试                                        ║");
        std::println("  ╚══════════════════════════════════════════════════════════╝");
        std::println("");
        std::println("  目标: {}:{}", test_host, test_port);
        std::println("");

        /* Step 1: DNS 解析 */
        std::println("  [1/4] DNS 解析...");
        auto dns_start = std::chrono::steady_clock::now();
        struct hostent* he = gethostbyname(test_host);
        auto dns_end = std::chrono::steady_clock::now();
        auto dns_us = std::chrono::duration_cast<std::chrono::microseconds>(dns_end - dns_start).count();
        double dns_time = dns_us / 1000.0;

        if (!he) {
            std::println("        ✗ DNS 解析失败: {}", test_host);
#ifdef _WIN32
            std::println("        错误码: {}", WSAGetLastError());
#endif
            return -1;
        }

        struct in_addr addr{};
        std::memcpy(&addr, he->h_addr_list[0], sizeof(addr));
        std::println("        ✓ DNS 解析成功: {} -> {} ({:.1f} ms)",
                     test_host, inet_ntoa(addr), dns_time);
        std::println("");

        /* Step 2: TCP 连接 */
        std::println("  [2/4] TCP 连接...");
#ifdef _WIN32
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
        socket_t sock = socket(AF_INET, SOCK_STREAM, 0);
        if (!ISVALIDSOCKET(sock)) {
            std::println("        ✗ 创建 socket 失败");
            return -1;
        }

        struct sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(static_cast<unsigned short>(test_port));
        server_addr.sin_addr = *reinterpret_cast<struct in_addr*>(he->h_addr);

#ifdef _WIN32
        {
            unsigned long mode = 1;
            ioctlsocket(sock, FIONBIO, &mode);
        }
#else
        {
            int flags = fcntl(sock, F_GETFL, 0);
            fcntl(sock, F_SETFL, flags | O_NONBLOCK);
        }
#endif

        auto tcp_start = std::chrono::steady_clock::now();
        int conn_ret = connect(sock, reinterpret_cast<struct sockaddr*>(&server_addr),
                                sizeof(server_addr));
        if (conn_ret < 0) {
#ifdef _WIN32
            if (WSAGetLastError() != WSAEWOULDBLOCK) {
#else
            if (errno != EINPROGRESS) {
#endif
                std::println("        ✗ TCP 连接失败");
                CLOSE_SOCKET(sock);
#ifdef _WIN32
                WSACleanup();
#endif
                return -1;
            }
            /* 等待连接完成 */
            fd_set fdset;
            FD_ZERO(&fdset);
            FD_SET(sock, &fdset);
            struct timeval tv{};
            tv.tv_sec = 3;
            tv.tv_usec = 0;
            if (select(static_cast<int>(sock) + 1, nullptr, &fdset, nullptr, &tv) <= 0) {
                std::println("        ✗ TCP 连接超时 (3s)");
                CLOSE_SOCKET(sock);
#ifdef _WIN32
                WSACleanup();
#endif
                return -1;
            }
        }
        auto tcp_end = std::chrono::steady_clock::now();
        auto tcp_us = std::chrono::duration_cast<std::chrono::microseconds>(tcp_end - tcp_start).count();
        double tcp_time = tcp_us / 1000.0;

        std::println("        ✓ TCP 连接成功 ({:.1f} ms)", tcp_time);
        std::println("");

        /* Step 3: TLS 握手 (可选) */
        std::println("  [3/4] TLS 握手...");
        auto tls_start = std::chrono::steady_clock::now();

        void* ssl = nullptr;
        if (tls_client_init(nullptr) != 0) {
            std::println("        ⚠ TLS 初始化失败: {}", tls_last_error());
            std::println("        (仅 TCP 连接可用, 无 TLS)");
        } else if (tls_client_connect(&ssl, test_host,
                                       static_cast<unsigned short>(test_port)) != 0) {
            std::println("        ⚠ TLS 握手失败: {}", tls_last_error());
        } else {
            auto tls_end = std::chrono::steady_clock::now();
            auto tls_us = std::chrono::duration_cast<std::chrono::microseconds>(tls_end - tls_start).count();
            double tls_time = tls_us / 1000.0;

            std::println("        ✓ TLS 握手成功 ({:.1f} ms)", tls_time);

            char tls_info[2048]{};
            tls_get_info(ssl, tls_info, sizeof(tls_info));
            std::println("");
            std::println("        TLS 详情:");
            char* line = std::strtok(tls_info, "\n");
            while (line) {
                std::println("          {}", line);
                line = std::strtok(nullptr, "\n");
            }

            tls_close(ssl);
        }
        std::println("");

        /* Step 4: HTTP 请求测试 */
        std::println("  [4/4] HTTP 请求测试...");
        /* 临时切换目标 */
        std::string orig_host = cli::g_cli_config.host;
        int orig_port = cli::g_cli_config.port;

        cli::g_cli_config.host = test_host;
        cli::g_cli_config.port = test_port;

        auto http_start = std::chrono::steady_clock::now();
        char response[8192]{};
        int http_ret = http_request("GET", "/api/health", nullptr, nullptr,
                                     response, sizeof(response));
        auto http_end = std::chrono::steady_clock::now();
        auto http_us = std::chrono::duration_cast<std::chrono::microseconds>(http_end - http_start).count();
        double http_time = http_us / 1000.0;

        /* 恢复配置 */
        cli::g_cli_config.host = orig_host;
        cli::g_cli_config.port = orig_port;

        if (http_ret == 0) {
            int http_status = http_get_status(response);
            std::println("        ✓ HTTP GET /api/health -> {} ({:.1f} ms)",
                         http_status, http_time);
        } else {
            std::println("        ⚠ HTTP 请求失败: {}", tls_last_error());
        }
        std::println("");

        std::println("  ┌─────────────────────────────────────────────────────────┐");
        std::println("  │ 测试摘要                                                │");
        std::println("  ├─────────────────────────────────────────────────────────┤");
        std::println("  │ DNS:     ✓ {:.1f} ms                                    │", dns_time);
        std::println("  │ TCP:     ✓ {:.1f} ms                                    │", tcp_time);
        std::println("  │ TLS:     {}                                              │", ssl ? "✓" : "✗");
        std::println("  │ HTTP:    {}                                              │", http_ret == 0 ? "✓" : "✗");
        std::println("  └─────────────────────────────────────────────────────────┘");
        std::println("");

        CLOSE_SOCKET(sock);
#ifdef _WIN32
        WSACleanup();
#endif
        return (http_ret == 0) ? 0 : -1;

    } else {
        std::println(stderr, "未知 network 子命令: {}", subcmd);
        return -1;
    }
}

extern "C" int init_cmd_network(void)
{
    register_command("network",
        "网络连通性诊断 (DNS/TCP/TLS/HTTP)",
        "network test <host> <port>",
        cmd_network);
    return 0;
}
