/**
 * Chrono-shift TLS 桩实现 (无 OpenSSL 环境使用)
 * 语言标准: C99
 *
 * 当 OpenSSL 库不可用时，提供空操作实现
 * 注意: 仅用于开发/测试，生产环境必须链接 OpenSSL
 */

#include "tls_server.h"
#include "server.h"
#include "platform_compat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * 服务端 API (桩实现)
 * ============================================================ */

int tls_server_init(const char* cert_file, const char* key_file)
{
    (void)cert_file;
    (void)key_file;
    LOG_WARN("TLS 桩模式: tls_server_init (无加密)");
    return 0;
}

SSL* tls_server_wrap(int fd)
{
    (void)fd;
    /* 返回非空指针表示成功 (实际不使用) */
    return (SSL*)(intptr_t)1;
}

void tls_close(SSL* ssl)
{
    (void)ssl;
    /* 桩实现: 无操作 */
}

int tls_server_auto_init(const char* cert_dir)
{
    (void)cert_dir;
    LOG_WARN("TLS 桩模式: tls_server_auto_init (无加密)");
    return 0;
}

void tls_server_cleanup(void)
{
    /* 桩实现: 无操作 */
}

bool tls_server_is_enabled(void)
{
    return true; /* 报告已启用 */
}

/* ============================================================
 * 客户端 API (桩实现)
 * ============================================================ */

int tls_client_init(const char* ca_file)
{
    (void)ca_file;
    LOG_WARN("TLS 桩模式: tls_client_init (无加密)");
    return 0;
}

int tls_client_connect(SSL** ssl_out, const char* host, uint16_t port)
{
    if (!ssl_out || !host) return -1;

    /* 直接建立 TCP 连接 (无 TLS) */
#ifdef PLATFORM_WINDOWS
    SOCKET fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == INVALID_SOCKET) return -1;
#else
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
#endif

    /* 返回 socket 描述符 */
    *ssl_out = (SSL*)(intptr_t)(fd + 1000); /* 用偏移量区分 */
    return fd;
}

void tls_client_cleanup(void)
{
    /* 桩实现: 无操作 */
}

/* ============================================================
 * 通用 I/O API (桩实现)
 * ============================================================ */

int tls_read(SSL* ssl, void* buf, int len)
{
    if (!ssl || !buf || len <= 0) return -1;

    /* 从包装的 fd 读取 */
    intptr_t fd_val = (intptr_t)ssl;
    int fd;
    if (fd_val >= 1000) {
        fd = (int)(fd_val - 1000);
    } else {
        fd = (int)fd_val;
    }

#ifdef PLATFORM_WINDOWS
    return recv(fd, (char*)buf, len, 0);
#else
    return (int)read(fd, buf, (size_t)len);
#endif
}

int tls_write(SSL* ssl, const void* buf, int len)
{
    if (!ssl || !buf || len <= 0) return -1;

    intptr_t fd_val = (intptr_t)ssl;
    int fd;
    if (fd_val >= 1000) {
        fd = (int)(fd_val - 1000);
    } else {
        fd = (int)fd_val;
    }

#ifdef PLATFORM_WINDOWS
    return send(fd, (const char*)buf, len, 0);
#else
    return (int)write(fd, buf, (size_t)len);
#endif
}

void tls_get_info(SSL* ssl, char* out_buf, size_t buf_size)
{
    (void)ssl;
    if (out_buf && buf_size > 0) {
        snprintf(out_buf, buf_size, "TLS (桩模式 - 无加密)");
    }
}

const char* tls_last_error(void)
{
    return "TLS 桩模式下无错误";
}
