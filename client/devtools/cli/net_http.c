/**
 * net_http.c — 开发者模式 CLI HTTP 网络层
 *
 * 自包含的 HTTP/HTTPS 客户端实现
 * 支持 TCP 明文和 OpenSSL TLS 加密连接
 *
 * 从 client/tools/debug_cli.c 重构提取
 * 依赖: devtools_cli.h (g_config), tls_client.c (可选 TLS)
 */
#include "devtools_cli.h"

#ifndef _WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#endif

/* ============================================================
 * 平台抽象
 * ============================================================ */

#ifdef _WIN32
    typedef SOCKET socket_t;
    #define ISVALIDSOCKET(s) ((s) != INVALID_SOCKET)
    #define CLOSE_SOCKET(s)  closesocket(s)
#else
    typedef int socket_t;
    #define INVALID_SOCKET   (-1)
    #define ISVALIDSOCKET(s) ((s) >= 0)
    #define CLOSE_SOCKET(s)  close(s)
#endif

#ifndef BUFFER_SIZE
#define BUFFER_SIZE 65536
#endif

/* TLS 外部函数 (来自 client/src/network/tls_client.c) */
extern int tls_client_init(const char* cert_dir);
extern int tls_client_connect(void** ssl, const char* host, unsigned short port);
extern int tls_write(void* ssl, const char* data, size_t len);
extern int tls_read(void* ssl, char* buf, size_t len);
extern void tls_close(void* ssl);
extern const char* tls_last_error(void);

/* ============================================================
 * TCP 连接
 * ============================================================ */

static socket_t tcp_connect(const char* host, int port)
{
    struct hostent* he;
    struct sockaddr_in addr;
    socket_t fd;

    he = gethostbyname(host);
    if (!he) {
        fprintf(stderr, "[-] 无法解析主机: %s\n", host);
        return INVALID_SOCKET;
    }

    fd = (socket_t)socket(AF_INET, SOCK_STREAM, 0);
    if (!ISVALIDSOCKET(fd)) {
        fprintf(stderr, "[-] 创建 socket 失败\n");
        return INVALID_SOCKET;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)port);
    memcpy(&addr.sin_addr, he->h_addr_list[0], (size_t)he->h_length);

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        fprintf(stderr, "[-] 连接 %s:%d 失败\n", host, port);
        CLOSE_SOCKET(fd);
        return INVALID_SOCKET;
    }

    return fd;
}

/* ============================================================
 * HTTP 请求
 * ============================================================ */

/**
 * 发送 HTTP/HTTPS 请求并接收完整响应
 *
 * @param method       HTTP 方法 (GET/POST/PUT/DELETE 等)
 * @param path         请求路径 (如 /api/health)
 * @param body         请求体 (可以为 NULL)
 * @param content_type Content-Type (可以为 NULL, 默认 application/json)
 * @param response     输出缓冲区
 * @param resp_size    输出缓冲区大小
 * @return 0=成功, -1=失败
 */
int http_request(
    const char* method,
    const char* path,
    const char* body,
    const char* content_type,
    char* response,
    size_t resp_size)
{
    char request[BUFFER_SIZE];
    int len;

    if (body && strlen(body) > 0) {
        len = snprintf(request, sizeof(request),
            "%s %s HTTP/1.1\r\n"
            "Host: %s:%d\r\n"
            "Content-Type: %s\r\n"
            "Content-Length: %zu\r\n"
            "Connection: close\r\n"
            "\r\n"
            "%s",
            method, path,
            g_config.host, g_config.port,
            content_type ? content_type : "application/json",
            strlen(body),
            body);
    } else {
        len = snprintf(request, sizeof(request),
            "%s %s HTTP/1.1\r\n"
            "Host: %s:%d\r\n"
            "Connection: close\r\n"
            "\r\n",
            method, path,
            g_config.host, g_config.port);
    }

    if (len < 0 || (size_t)len >= sizeof(request)) {
        fprintf(stderr, "[-] 请求过长\n");
        return -1;
    }

    if (g_config.verbose) {
        printf("[*] 发送请求:\n%s\n", request);
    }

    if (g_config.use_tls) {
        /* === HTTPS 模式: 使用 tls_client_connect === */
        void* ssl = NULL;
        if (tls_client_init(NULL) != 0) {
            fprintf(stderr, "[-] TLS 客户端初始化失败: %s\n", tls_last_error());
            return -1;
        }
        if (tls_client_connect(&ssl, g_config.host, (unsigned short)g_config.port) != 0) {
            fprintf(stderr, "[-] TLS 连接 %s:%d 失败: %s\n",
                    g_config.host, g_config.port, tls_last_error());
            return -1;
        }

        /* 发送请求 (TLS) */
        int sent = 0;
        while (sent < len) {
            int n = (int)tls_write(ssl, request + sent, (size_t)(len - sent));
            if (n < 0) {
                fprintf(stderr, "[-] TLS 发送失败: %s\n", tls_last_error());
                tls_close(ssl);
                return -1;
            }
            sent += n;
        }

        /* 接收响应 (TLS) */
        size_t total = 0;
        int n;
        while (total < resp_size - 1) {
            n = (int)tls_read(ssl, response + total, resp_size - 1 - total);
            if (n < 0) {
                fprintf(stderr, "[-] TLS 接收失败: %s\n", tls_last_error());
                tls_close(ssl);
                return -1;
            }
            if (n == 0) break; /* 连接关闭 */
            total += (size_t)n;
        }
        response[total] = 0;

        tls_close(ssl);

        if (total == 0) {
            fprintf(stderr, "[-] 未收到响应\n");
            return -1;
        }
        return 0;

    } else {
        /* === HTTP 模式: 原始 TCP === */
        socket_t fd = tcp_connect(g_config.host, g_config.port);
        if (!ISVALIDSOCKET(fd)) return -1;

        /* 发送请求 */
        int sent = 0;
        while (sent < len) {
            int n = (int)send(fd, request + sent, (size_t)(len - sent), 0);
            if (n <= 0) {
                fprintf(stderr, "[-] 发送请求失败\n");
                CLOSE_SOCKET(fd);
                return -1;
            }
            sent += n;
        }

        /* 接收响应 */
        size_t total = 0;
        int n;
        while (total < resp_size - 1) {
            n = (int)recv(fd, response + total, resp_size - 1 - total, 0);
            if (n <= 0) break;
            total += (size_t)n;
        }
        response[total] = 0;

        CLOSE_SOCKET(fd);

        if (total == 0) {
            fprintf(stderr, "[-] 未收到响应\n");
            return -1;
        }
        return 0;
    }
}

/* ============================================================
 * HTTP 响应解析工具
 * ============================================================ */

/**
 * 从 HTTP 响应中提取消息体 (在 \r\n\r\n 之后)
 */
const char* http_get_body(const char* response)
{
    const char* body = strstr(response, "\r\n\r\n");
    if (body) return body + 4;
    return response;
}

/**
 * 解析 HTTP 响应状态码
 * @return 状态码 (如 200, 404), -1=解析失败
 */
int http_get_status(const char* response)
{
    int code = 0;
    if (sscanf(response, "HTTP/1.%*d %d", &code) == 1 ||
        sscanf(response, "HTTP/%*d.%*d %d", &code) == 1) {
        return code;
    }
    return -1;
}
