/**
 * cmd_network.c — 网络诊断命令
 * 对应 debug_cli.c:2635 cmd_network
 *
 * 完整的 DNS/TCP/TLS/HTTP 四层连通性诊断
 */
#include "../devtools_cli.h"
#include <time.h>

#ifdef _WIN32
    #include <winsock2.h>
    #include <windows.h>
    #include <ws2tcpip.h>
    #define CLOSE_SOCKET(x) closesocket(x)
    #define ISVALIDSOCKET(s) ((s) != INVALID_SOCKET)
    typedef SOCKET socket_t;
#else
    #include <unistd.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netdb.h>
    #include <arpa/inet.h>
    #include <errno.h>
    #include <fcntl.h>
    #define CLOSE_SOCKET(x) close(x)
    #define ISVALIDSOCKET(s) ((s) >= 0)
    typedef int socket_t;
#endif

/* ============================================================
 * tls_client / http_request 函数
 * ============================================================ */
extern int tls_client_init(const char* cert_dir);
extern int tls_client_connect(void** ssl, const char* host, unsigned short port);
extern void tls_close(void* ssl);
extern const char* tls_last_error(void);
extern void tls_get_info(void* ssl, char* buf, size_t buf_size);
extern int http_request(const char* method, const char* path,
                        const char* headers, const char* body,
                        char* response, size_t resp_size);
extern int http_get_status(const char* response);

/* ============================================================
 * network 命令 - 网络连通性测试
 * ============================================================ */
static int cmd_network(int argc, char** argv)
{
    if (argc < 1) {
        printf("用法:\n");
        printf("  network test <host> <port>     - 网络连通性测试\n");
        printf("    测试目标主机的 TCP 连接和 TLS 握手\n");
        return -1;
    }

    const char* subcmd = argv[0];

    if (strcmp(subcmd, "test") == 0) {
        if (argc < 3) {
            fprintf(stderr, "用法: network test <host> <port>\n");
            fprintf(stderr, "示例: network test 127.0.0.1 4443\n");
            return -1;
        }

        const char* test_host = argv[1];
        int test_port = atoi(argv[2]);
        if (test_port <= 0 || test_port > 65535) {
            fprintf(stderr, "[-] 无效端口: %d\n", test_port);
            return -1;
        }

        printf("\n");
        printf("  ╔══════════════════════════════════════════════════════════╗\n");
        printf("  ║     网络连通性测试                                        ║\n");
        printf("  ╚══════════════════════════════════════════════════════════╝\n");
        printf("\n");
        printf("  目标: %s:%d\n", test_host, test_port);
        printf("\n");

        /* Step 1: DNS 解析 */
        printf("  [1/4] DNS 解析...\n");
        clock_t dns_start = clock();
        struct hostent* he = gethostbyname(test_host);
        clock_t dns_end = clock();
        double dns_time = ((double)(dns_end - dns_start)) / CLOCKS_PER_SEC * 1000.0;

        if (!he) {
            printf("        ✗ DNS 解析失败: %s\n", test_host);
#ifdef _WIN32
            printf("        错误码: %d\n", WSAGetLastError());
#endif
            return -1;
        }

        struct in_addr addr;
        memcpy(&addr, he->h_addr_list[0], sizeof(addr));
        printf("        ✓ DNS 解析成功: %s -> %s (%.1f ms)\n",
               test_host, inet_ntoa(addr), dns_time);
        printf("\n");

        /* Step 2: TCP 连接 */
        printf("  [2/4] TCP 连接...\n");
#ifdef _WIN32
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
        socket_t sock = socket(AF_INET, SOCK_STREAM, 0);
        if (!ISVALIDSOCKET(sock)) {
            printf("        ✗ 创建 socket 失败\n");
            return -1;
        }

        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons((unsigned short)test_port);
        server_addr.sin_addr = *(struct in_addr*)he->h_addr;

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

        clock_t tcp_start = clock();
        int conn_ret = connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr));
        if (conn_ret < 0) {
#ifdef _WIN32
            if (WSAGetLastError() != WSAEWOULDBLOCK) {
#else
            if (errno != EINPROGRESS) {
#endif
                printf("        ✗ TCP 连接失败\n");
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
            struct timeval tv;
            tv.tv_sec = 3;
            tv.tv_usec = 0;
            if (select((int)sock + 1, NULL, &fdset, NULL, &tv) <= 0) {
                printf("        ✗ TCP 连接超时 (3s)\n");
                CLOSE_SOCKET(sock);
#ifdef _WIN32
                WSACleanup();
#endif
                return -1;
            }
        }
        clock_t tcp_end = clock();
        double tcp_time = ((double)(tcp_end - tcp_start)) / CLOCKS_PER_SEC * 1000.0;

        printf("        ✓ TCP 连接成功 (%.1f ms)\n", tcp_time);
        printf("\n");

        /* Step 3: TLS 握手 (可选) */
        printf("  [3/4] TLS 握手...\n");
        clock_t tls_start = clock();

        void* ssl = NULL;
        if (tls_client_init(NULL) != 0) {
            printf("        ⚠ TLS 初始化失败: %s\n", tls_last_error());
            printf("        (仅 TCP 连接可用, 无 TLS)\n");
        } else if (tls_client_connect(&ssl, test_host, (unsigned short)test_port) != 0) {
            printf("        ⚠ TLS 握手失败: %s\n", tls_last_error());
        } else {
            clock_t tls_end = clock();
            double tls_time = ((double)(tls_end - tls_start)) / CLOCKS_PER_SEC * 1000.0;

            printf("        ✓ TLS 握手成功 (%.1f ms)\n", tls_time);

            char tls_info[2048] = {0};
            tls_get_info(ssl, tls_info, sizeof(tls_info));
            printf("\n        TLS 详情:\n");
            /* 格式化输出 TLS 信息 */
            char* line = strtok(tls_info, "\n");
            while (line) {
                printf("          %s\n", line);
                line = strtok(NULL, "\n");
            }

            tls_close(ssl);
        }
        printf("\n");

        /* Step 4: HTTP 请求测试 */
        printf("  [4/4] HTTP 请求测试...\n");
        /* 临时切换目标 */
        char orig_host[256];
        int orig_port = g_config.port;
        snprintf(orig_host, sizeof(orig_host), "%s", g_config.host);

        snprintf(g_config.host, sizeof(g_config.host), "%s", test_host);
        g_config.port = test_port;

        clock_t http_start = clock();
        char response[8192] = {0};
        int http_ret = http_request("GET", "/api/health", NULL, NULL,
                                     response, sizeof(response));
        clock_t http_end = clock();
        double http_time = ((double)(http_end - http_start)) / CLOCKS_PER_SEC * 1000.0;

        /* 恢复配置 */
        snprintf(g_config.host, sizeof(g_config.host), "%s", orig_host);
        g_config.port = orig_port;

        if (http_ret == 0) {
            int http_status = http_get_status(response);
            printf("        ✓ HTTP GET /api/health -> %d (%.1f ms)\n",
                   http_status, http_time);
        } else {
            printf("        ⚠ HTTP 请求失败: %s\n", tls_last_error());
        }
        printf("\n");

        printf("  ┌─────────────────────────────────────────────────────────┐\n");
        printf("  │ 测试摘要                                                │\n");
        printf("  ├─────────────────────────────────────────────────────────┤\n");
        printf("  │ DNS:     ✓ %.1f ms                                    │\n", dns_time);
        printf("  │ TCP:     ✓ %.1f ms                                    │\n", tcp_time);
        printf("  │ TLS:     %s                                              │\n", ssl ? "✓" : "✗");
        printf("  │ HTTP:    %s                                              │\n", http_ret == 0 ? "✓" : "✗");
        printf("  └─────────────────────────────────────────────────────────┘\n");
        printf("\n");

        CLOSE_SOCKET(sock);
#ifdef _WIN32
        WSACleanup();
#endif
        return (http_ret == 0) ? 0 : -1;

    } else {
        fprintf(stderr, "未知 network 子命令: %s\n", subcmd);
        return -1;
    }
}

int init_cmd_network(void)
{
    register_command("network",
        "网络连通性诊断 (DNS/TCP/TLS/HTTP)",
        "network test <host> <port>",
        cmd_network);
    return 0;
}
