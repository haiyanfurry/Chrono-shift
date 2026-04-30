/**
 * Chrono-shift TLS 桩实现
 * 当系统未安装 OpenSSL 开发库时，提供空函数替代
 * 编译时使用 -DHAS_TLS 来启用真实 TLS 支持
 *
 * 语言标准: C99
 * 平台: 跨平台 (Linux / Windows)
 */

#include "tls_server.h"
#include <stdio.h>
#include <string.h>

/* ============================================================
 * 线程本地错误信息
 * ============================================================ */
static char g_stub_error[256] = "TLS 支持未编译 (需要 OpenSSL 和 -DHAS_TLS)";

/* ============================================================
 * 服务端 API
 * ============================================================ */

int tls_server_init(const char* cert_file, const char* key_file)
{
    (void)cert_file;
    (void)key_file;
    snprintf(g_stub_error, sizeof(g_stub_error),
             "TLS 未启用: 编译时未包含 OpenSSL 支持");
    return -1;
}

SSL* tls_server_wrap(int fd)
{
    (void)fd;
    return NULL;
}

void tls_close(SSL* ssl)
{
    (void)ssl;
    /* stub: nothing to do */
}

void tls_server_cleanup(void)
{
    /* stub: nothing to do */
}

bool tls_server_is_enabled(void)
{
    return false;
}

/* ============================================================
 * 客户端 API
 * ============================================================ */

int tls_client_init(const char* ca_file)
{
    (void)ca_file;
    snprintf(g_stub_error, sizeof(g_stub_error),
             "TLS 未启用: 编译时未包含 OpenSSL 支持");
    return -1;
}

int tls_client_connect(SSL** ssl_out, const char* host, uint16_t port)
{
    (void)ssl_out;
    (void)host;
    (void)port;
    return -1;
}

void tls_client_cleanup(void)
{
    /* stub: nothing to do */
}

/* ============================================================
 * 通用 I/O API
 * ============================================================ */

int tls_read(SSL* ssl, void* buf, int len)
{
    (void)ssl;
    (void)buf;
    (void)len;
    return -1;
}

int tls_write(SSL* ssl, const void* buf, int len)
{
    (void)ssl;
    (void)buf;
    (void)len;
    return -1;
}

void tls_get_info(SSL* ssl, char* out_buf, size_t buf_size)
{
    (void)ssl;
    if (out_buf && buf_size > 0) {
        snprintf(out_buf, buf_size, "TLS not available (stub)");
    }
}

const char* tls_last_error(void)
{
    return g_stub_error;
}
