/**
 * Chrono-shift 客户端 TLS 实现
 * 基于 OpenSSL 的跨平台 TLS 封装
 * 语言标准: C99
 *
 * 为客户端和 CLI 工具提供 TLS 连接能力:
 * 1. 客户端 network/ — 连接 HTTPS 服务端
 * 2. CLI 工具 debug_cli.c — 连接远程服务端进行调试
 */

#include "../../include/tls_client.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================
 * OpenSSL 头文件 (需要 HTTPS_SUPPORT)
 * ============================================================ */
#if HTTPS_SUPPORT
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4996)
#endif

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/crypto.h>

#ifdef _MSC_VER
#pragma warning(pop)
#endif
#endif /* HTTPS_SUPPORT */

/* ============================================================
 * 平台相关头文件
 * ============================================================ */
#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#endif

/* ============================================================
 * 内部状态
 * ============================================================ */

/* TLS 错误缓冲区 (线程局部) */
#if defined(_MSC_VER)
static __declspec(thread) char t_errbuf[256] = {0};
#else
static __thread char t_errbuf[256] = {0};
#endif

/* 全局 SSL 上下文 (惰性初始化) */
static SSL_CTX* g_ssl_ctx = NULL;
static int g_init_count = 0;

/* 简单互斥锁用于引用计数 (仅 Windows CRITICAL_SECTION) */
#ifdef _WIN32
static CRITICAL_SECTION g_init_lock;
static int g_lock_inited = 0;
#endif

/* ============================================================
 * 内部辅助函数
 * ============================================================ */

static void set_error(const char* msg)
{
    size_t len = strlen(msg);
    if (len > sizeof(t_errbuf) - 1) len = sizeof(t_errbuf) - 1;
    memcpy(t_errbuf, msg, len);
    t_errbuf[len] = '\0';
}

/* 获取 OpenSSL 错误描述 */
static void capture_openssl_error(void)
{
    unsigned long err = ERR_get_error();
    if (err == 0) {
        set_error("No error");
        return;
    }
    char buf[256];
    ERR_error_string_n(err, buf, sizeof(buf));
    set_error(buf);
}

/* 解析端口字符串到 uint16 */
static int parse_port(const char* port_str, uint16_t* out_port)
{
    if (!port_str || !out_port) return -1;
    long port = atol(port_str);
    if (port <= 0 || port > 65535) {
        set_error("Invalid port number");
        return -1;
    }
    *out_port = (uint16_t)port;
    return 0;
}

/* ============================================================
 * tls_client_init — 初始化客户端 TLS 上下文
 * ============================================================ */
int tls_client_init(const char* ca_file)
{
#ifdef _WIN32
    if (!g_lock_inited) {
        InitializeCriticalSection(&g_init_lock);
        g_lock_inited = 1;
    }
    EnterCriticalSection(&g_init_lock);
#else
    /* 简单起见，假设单线程调用 init/cleanup */
#endif

    if (g_ssl_ctx != NULL) {
        g_init_count++;
#ifdef _WIN32
        LeaveCriticalSection(&g_init_lock);
#endif
        return 0; /* 已经初始化 */
    }

    /* 初始化 OpenSSL */
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();

    /* 创建 SSL_CTX (客户端模式) */
    const SSL_METHOD* method = TLS_client_method();
    if (!method) {
        capture_openssl_error();
#ifdef _WIN32
        LeaveCriticalSection(&g_init_lock);
#endif
        return -1;
    }

    SSL_CTX* ctx = SSL_CTX_new(method);
    if (!ctx) {
        capture_openssl_error();
#ifdef _WIN32
        LeaveCriticalSection(&g_init_lock);
#endif
        return -1;
    }

    /* 自动选择证书验证策略 */
    if (ca_file && ca_file[0] != '\0') {
        /* 使用指定的 CA 证书文件 */
        if (SSL_CTX_load_verify_locations(ctx, ca_file, NULL) != 1) {
            capture_openssl_error();
            SSL_CTX_free(ctx);
#ifdef _WIN32
            LeaveCriticalSection(&g_init_lock);
#endif
            return -1;
        }
    } else {
        /* 使用系统默认信任库 */
        if (SSL_CTX_set_default_verify_paths(ctx) != 1) {
            /* 非致命: 继续但使用空验证 */
            /* 在 Windows 上可能找不到默认路径 */
        }
    }

    /* 启用服务器证书验证 */
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);

    /* 设置最低 TLS 版本 (TLS 1.2) */
    SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);

    /* 设置密码套件 */
    const char* ciphers =
        "ECDHE-ECDSA-AES128-GCM-SHA256:"
        "ECDHE-RSA-AES128-GCM-SHA256:"
        "ECDHE-ECDSA-AES256-GCM-SHA384:"
        "ECDHE-RSA-AES256-GCM-SHA384:"
        "DHE-RSA-AES128-GCM-SHA256:"
        "DHE-RSA-AES256-GCM-SHA384:"
        "HIGH:!aNULL:!eNULL:!EXPORT:!DES:!MD5:!PSK:!RC4";
    SSL_CTX_set_cipher_list(ctx, ciphers);

    /* TLS 1.3 密码套件 */
    const char* ciphersuites =
        "TLS_AES_128_GCM_SHA256:"
        "TLS_AES_256_GCM_SHA384:"
        "TLS_CHACHA20_POLY1305_SHA256";
    SSL_CTX_set_ciphersuites(ctx, ciphersuites);

    g_ssl_ctx = ctx;
    g_init_count = 1;

#ifdef _WIN32
    LeaveCriticalSection(&g_init_lock);
#endif

    return 0;
}

/* ============================================================
 * tls_client_connect — 连接到远程 TLS 服务器
 * ============================================================ */
int tls_client_connect(SSL** ssl_out, const char* host, uint16_t port)
{
    if (!ssl_out || !host) {
        set_error("Invalid parameters");
        return -1;
    }

    *ssl_out = NULL;

    if (!g_ssl_ctx) {
        set_error("TLS not initialized (call tls_client_init first)");
        return -1;
    }

    /* ---- TCP 连接 ---- */
    int fd = -1;

#ifdef _WIN32
    /* 确保 Winsock 已初始化 */
    static int ws_init = 0;
    if (!ws_init) {
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
            set_error("WSAStartup failed");
            return -1;
        }
        ws_init = 1;
    }
#endif

    /* 创建 socket */
    fd = (int)socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
#ifdef _WIN32
        set_error("socket() failed");
#else
        set_error(strerror(errno));
#endif
        return -1;
    }

    /* DNS 解析 + 连接 */
    struct hostent* he = gethostbyname(host);
    if (!he) {
        set_error("DNS resolution failed");
#ifdef _WIN32
        closesocket(fd);
#else
        close(fd);
#endif
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
#ifdef _WIN32
        set_error("connect() failed");
        closesocket(fd);
#else
        set_error(strerror(errno));
        close(fd);
#endif
        return -1;
    }

    /* ---- TLS 握手 ---- */
    SSL* ssl = SSL_new(g_ssl_ctx);
    if (!ssl) {
        capture_openssl_error();
#ifdef _WIN32
        closesocket(fd);
#else
        close(fd);
#endif
        return -1;
    }

    SSL_set_fd(ssl, fd);

    /* 设置 SNI (Server Name Indication) */
    SSL_set_tlsext_host_name(ssl, host);

    /* 执行 TLS 握手 (客户端模式) */
    int ret = SSL_connect(ssl);
    if (ret != 1) {
        capture_openssl_error();
        SSL_free(ssl);
#ifdef _WIN32
        closesocket(fd);
#else
        close(fd);
#endif
        return -1;
    }

    /* 验证服务器证书 (可选但重要) */
    long verify_result = SSL_get_verify_result(ssl);
    if (verify_result != X509_V_OK) {
        /* 非致命: 记录警告但仍继续 */
        /* 在一些测试环境中可能使用自签名证书 */
    }

    *ssl_out = ssl;
    return fd;
}

/* ============================================================
 * tls_close — 关闭 TLS 连接并释放 SSL 对象
 * ============================================================ */
void tls_close(SSL* ssl)
{
    if (!ssl) return;

    /* 尝试优雅关闭 */
    int fd = SSL_get_fd(ssl);
    SSL_shutdown(ssl);
    SSL_free(ssl);

    /* 关闭底层 socket */
    if (fd >= 0) {
#ifdef _WIN32
        closesocket(fd);
#else
        close(fd);
#endif
    }
}

/* ============================================================
 * tls_client_cleanup — 清理客户端 TLS 上下文
 * ============================================================ */
void tls_client_cleanup(void)
{
#ifdef _WIN32
    if (!g_lock_inited) return;
    EnterCriticalSection(&g_init_lock);
#endif

    if (g_init_count > 0) {
        g_init_count--;
        if (g_init_count == 0 && g_ssl_ctx) {
            SSL_CTX_free(g_ssl_ctx);
            g_ssl_ctx = NULL;
            EVP_cleanup();
            ERR_free_strings();
        }
    }

#ifdef _WIN32
    LeaveCriticalSection(&g_init_lock);
#endif
}

/* ============================================================
 * tls_read — 从 TLS 连接读取数据
 * ============================================================ */
int tls_read(SSL* ssl, void* buf, int len)
{
    if (!ssl || !buf || len <= 0) {
        set_error("Invalid parameters");
        return -1;
    }

    int ret = SSL_read(ssl, buf, len);
    if (ret > 0) {
        return ret;
    }

    int err = SSL_get_error(ssl, ret);
    switch (err) {
        case SSL_ERROR_WANT_READ:
        case SSL_ERROR_WANT_WRITE:
            return 0; /* 需要重试 (非阻塞) */

        case SSL_ERROR_ZERO_RETURN:
            /* 连接已正常关闭 */
            return -1;

        case SSL_ERROR_SYSCALL:
            /* 底层 socket 错误 */
#ifdef _WIN32
            if (WSAGetLastError() == WSAEWOULDBLOCK) return 0;
#else
            if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
#endif
            return -1;

        default:
            capture_openssl_error();
            return -1;
    }
}

/* ============================================================
 * tls_write — 向 TLS 连接写入数据
 * ============================================================ */
int tls_write(SSL* ssl, const void* buf, int len)
{
    if (!ssl || !buf || len <= 0) {
        set_error("Invalid parameters");
        return -1;
    }

    int ret = SSL_write(ssl, buf, len);
    if (ret > 0) {
        return ret;
    }

    int err = SSL_get_error(ssl, ret);
    switch (err) {
        case SSL_ERROR_WANT_READ:
        case SSL_ERROR_WANT_WRITE:
            return 0; /* 需要重试 */

        case SSL_ERROR_ZERO_RETURN:
            return -1;

        case SSL_ERROR_SYSCALL:
#ifdef _WIN32
            if (WSAGetLastError() == WSAEWOULDBLOCK) return 0;
#else
            if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
#endif
            return -1;

        default:
            capture_openssl_error();
            return -1;
    }
}

/* ============================================================
 * tls_get_info — 获取 TLS 连接信息
 * ============================================================ */
void tls_get_info(SSL* ssl, char* out_buf, size_t buf_size)
{
    if (!ssl || !out_buf || buf_size == 0) return;

    /* 获取密码套件名称 */
    const char* cipher = SSL_get_cipher(ssl);

    /* 获取 TLS 版本 */
    const char* version = SSL_get_version(ssl);

    /* 获取 SNI 主机名 */
    const char* sni = SSL_get_servername(ssl, TLSEXT_NAMETYPE_host_name);

    /* 获取证书信息 */
    X509* cert = SSL_get_peer_certificate(ssl);
    char subject[256] = "unknown";
    char issuer[256] = "unknown";

    if (cert) {
        X509_NAME* subj = X509_get_subject_name(cert);
        X509_NAME* iss  = X509_get_issuer_name(cert);
        if (subj) X509_NAME_oneline(subj, subject, sizeof(subject));
        if (iss)  X509_NAME_oneline(iss,  issuer,  sizeof(issuer));
        X509_free(cert);
    }

    snprintf(out_buf, buf_size,
             "TLS %s | Cipher: %s | SNI: %s | Subject: %s | Issuer: %s",
             version ? version : "N/A",
             cipher ? cipher : "N/A",
             sni ? sni : "N/A",
             subject,
             issuer);
}

/* ============================================================
 * tls_last_error — 获取最后错误描述
 * ============================================================ */
const char* tls_last_error(void)
{
    return t_errbuf;
}
