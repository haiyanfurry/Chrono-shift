#ifdef HAS_TLS

/**
 * Chrono-shift TLS 抽象层实现
 * 基于 OpenSSL 的跨平台 TLS 封装
 * 语言标准: C99
 *
 * 兼容 Linux (OpenSSL) 和 Windows (MinGW + OpenSSL)
 *
 * 编译链接:
 *   Linux:   gcc ... -lssl -lcrypto
 *   Windows: gcc ... -lssl -lcrypto -lws2_32
 */

#include "tls_server.h"
#include "platform_compat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* ============================================================
 * OpenSSL 头文件 (跨平台)
 * ============================================================ */
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/crypto.h>
#include <openssl/x509v3.h>

/* ============================================================
 * 全局状态
 * ============================================================ */

/* 服务端 SSL 上下文 */
static SSL_CTX* g_server_ctx = NULL;

/* 客户端 SSL 上下文 */
static SSL_CTX* g_client_ctx = NULL;

/* 最后一个错误消息 (线程局部存储简单实现) */
#if defined(_MSC_VER)
    static __declspec(thread) char g_last_error[256] = {0};
#else
    static __thread char g_last_error[256] = {0};
#endif

/* ============================================================
 * 内部工具
 * ============================================================ */

static void set_error(const char* msg)
{
    strncpy(g_last_error, msg, sizeof(g_last_error) - 1);
    g_last_error[sizeof(g_last_error) - 1] = '\0';
}

static void set_error_openssl(const char* prefix)
{
    unsigned long err = ERR_get_error();
    if (err) {
        char ssl_err[128];
        ERR_error_string_n(err, ssl_err, sizeof(ssl_err));
        snprintf(g_last_error, sizeof(g_last_error), "%s: %s", prefix, ssl_err);
    } else {
        set_error(prefix);
    }
}

/* ============================================================
 * 通用 I/O
 * ============================================================ */

int tls_read(SSL* ssl, void* buf, int len)
{
    if (!ssl) return -1;
    int ret = SSL_read(ssl, buf, len);
    if (ret <= 0) {
        int err = SSL_get_error(ssl, ret);
        if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
            /* 非阻塞模式下需重试, 返回 0 表示需要重试 */
            errno = EAGAIN;
            return 0;
        }
        set_error_openssl("SSL_read 失败");
    }
    return ret;
}

int tls_write(SSL* ssl, const void* buf, int len)
{
    if (!ssl) return -1;
    int ret = SSL_write(ssl, buf, len);
    if (ret <= 0) {
        int err = SSL_get_error(ssl, ret);
        if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
            errno = EAGAIN;
            return 0;
        }
        set_error_openssl("SSL_write 失败");
    }
    return ret;
}

void tls_get_info(SSL* ssl, char* out_buf, size_t buf_size)
{
    if (!ssl || !out_buf || buf_size < 1) return;
    out_buf[0] = '\0';

    const char* version = SSL_get_version(ssl);
    const char* cipher  = SSL_get_cipher(ssl);
    const char* sni     = SSL_get_servername(ssl, TLSEXT_NAMETYPE_host_name);

    if (sni) {
        snprintf(out_buf, buf_size, "TLS=%s, Cipher=%s, SNI=%s",
                 version ? version : "?", cipher ? cipher : "?", sni);
    } else {
        snprintf(out_buf, buf_size, "TLS=%s, Cipher=%s",
                 version ? version : "?", cipher ? cipher : "?");
    }
}

const char* tls_last_error(void)
{
    return g_last_error;
}

/* ============================================================
 * 服务端 API
 * ============================================================ */

int tls_server_init(const char* cert_file, const char* key_file)
{
    if (!cert_file || !key_file) {
        set_error("tls_server_init: 证书或密钥路径为空");
        return -1;
    }

    /* 初始化 OpenSSL (只需一次) */
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();

    /* 创建 SSL_CTX (TLS 1.2+) */
    const SSL_METHOD* method = TLS_server_method();
    g_server_ctx = SSL_CTX_new(method);
    if (!g_server_ctx) {
        set_error_openssl("SSL_CTX_new 失败");
        return -1;
    }

    /* 显式设置最低/最高 TLS 版本 (替代 SSL_OP_NO_* 方案) */
    if (!SSL_CTX_set_min_proto_version(g_server_ctx, TLS1_2_VERSION)) {
        fprintf(stderr, "[TLS] 警告: 设置最低 TLS 版本失败, 使用备用方案\n");
        SSL_CTX_set_options(g_server_ctx,
            SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 |
            SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1);
    }

    /* 优先使用服务端密码顺序 */
    SSL_CTX_set_options(g_server_ctx, SSL_OP_CIPHER_SERVER_PREFERENCE);

    /* 设置 TLS 1.2 及以下密码套件 (安全优先) */
    const char* ciphers =
        "ECDHE-ECDSA-AES128-GCM-SHA256:"
        "ECDHE-RSA-AES128-GCM-SHA256:"
        "ECDHE-ECDSA-AES256-GCM-SHA384:"
        "ECDHE-RSA-AES256-GCM-SHA384:"
        "ECDHE-ECDSA-CHACHA20-POLY1305:"
        "ECDHE-RSA-CHACHA20-POLY1305:"
        "DHE-RSA-AES128-GCM-SHA256:"
        "DHE-RSA-AES256-GCM-SHA384:"
        "!aNULL:!eNULL:!LOW:!MD5:!EXP:!RC4:!DES:!3DES";

    if (SSL_CTX_set_cipher_list(g_server_ctx, ciphers) <= 0) {
        set_error_openssl("SSL_CTX_set_cipher_list 失败");
        SSL_CTX_free(g_server_ctx);
        g_server_ctx = NULL;
        return -1;
    }

    /* 设置 TLS 1.3 密码套件 (OpenSSL 3.x 需要单独设置) */
    const char* ciphersuites =
        "TLS_AES_128_GCM_SHA256:"
        "TLS_AES_256_GCM_SHA384:"
        "TLS_CHACHA20_POLY1305_SHA256";
    if (SSL_CTX_set_ciphersuites(g_server_ctx, ciphersuites) <= 0) {
        set_error_openssl("SSL_CTX_set_ciphersuites 失败");
        SSL_CTX_free(g_server_ctx);
        g_server_ctx = NULL;
        return -1;
    }

    /* 加载证书链 (fullchain.pem) */
    if (SSL_CTX_use_certificate_chain_file(g_server_ctx, cert_file) <= 0) {
        set_error_openssl("加载证书失败");
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(g_server_ctx);
        g_server_ctx = NULL;
        return -1;
    }

    /* 加载私钥 */
    if (SSL_CTX_use_PrivateKey_file(g_server_ctx, key_file, SSL_FILETYPE_PEM) <= 0) {
        set_error_openssl("加载私钥失败");
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(g_server_ctx);
        g_server_ctx = NULL;
        return -1;
    }

    /* 验证私钥与证书匹配 */
    if (!SSL_CTX_check_private_key(g_server_ctx)) {
        set_error("私钥与证书不匹配");
        SSL_CTX_free(g_server_ctx);
        g_server_ctx = NULL;
        return -1;
    }

    printf("[TLS] 服务端初始化成功 (cert=%s, key=%s)\n", cert_file, key_file);
    return 0;
}

SSL* tls_server_wrap(int fd)
{
    if (!g_server_ctx) {
        set_error("TLS 未初始化, 请先调用 tls_server_init()");
        return NULL;
    }

    SSL* ssl = SSL_new(g_server_ctx);
    if (!ssl) {
        set_error_openssl("SSL_new 失败");
        return NULL;
    }

    /* 显式将 socket 设为阻塞模式 (Windows 下从非阻塞 listener accept
     * 的 socket 可能继承非阻塞属性, 导致 SSL_accept 失败) */
    set_blocking((socket_t)fd);

    SSL_set_fd(ssl, (int)fd);

    /* 执行 TLS 握手 */
    int ret = SSL_accept(ssl);
    if (ret <= 0) {
        int err = SSL_get_error(ssl, ret);
        /* 输出完整的 OpenSSL 错误队列 */
        fprintf(stderr, "[TLS] SSL_accept 失败: err=%d\n", err);
        unsigned long ssl_err;
        while ((ssl_err = ERR_get_error()) != 0) {
            char info[256];
            ERR_error_string_n(ssl_err, info, sizeof(info));
            fprintf(stderr, "  [TLS]   OpenSSL 错误: %s\n", info);
        }
        SSL_free(ssl);
        return NULL;
    }

    char buf[128];
    tls_get_info(ssl, buf, sizeof(buf));
    printf("[TLS] 新连接握手完成: %s\n", buf);

    return ssl;
}

void tls_close(SSL* ssl)
{
    if (ssl) {
        SSL_shutdown(ssl);
        SSL_free(ssl);
    }
}

void tls_server_cleanup(void)
{
    if (g_server_ctx) {
        SSL_CTX_free(g_server_ctx);
        g_server_ctx = NULL;
    }
    /* 注意: 不在此处调用 EVP_cleanup(), 因为客户端可能还在使用 */
}

bool tls_server_is_enabled(void)
{
    return (g_server_ctx != NULL);
}

/* ============================================================
 * 客户端 API
 * ============================================================ */

int tls_client_init(const char* ca_file)
{
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();

    const SSL_METHOD* method = TLS_client_method();
    g_client_ctx = SSL_CTX_new(method);
    if (!g_client_ctx) {
        set_error_openssl("SSL_CTX_new(客户端) 失败");
        return -1;
    }

    /* 禁用旧协议 */
    SSL_CTX_set_options(g_client_ctx,
        SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 |
        SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1);

    /* 如果指定了 CA 文件, 用于验证服务端证书 */
    if (ca_file) {
        if (SSL_CTX_load_verify_locations(g_client_ctx, ca_file, NULL) <= 0) {
            set_error_openssl("加载 CA 证书失败");
            SSL_CTX_free(g_client_ctx);
            g_client_ctx = NULL;
            return -1;
        }
    } else {
        /* 使用系统默认信任库 */
        if (SSL_CTX_set_default_verify_paths(g_client_ctx) <= 0) {
            /* 非致命: 某些系统可能没有默认路径 */
            fprintf(stderr, "[TLS] 警告: 无法加载系统信任库, 将跳过服务端证书验证\n");
        }
    }

    /* 默认验证服务端证书 (生产环境建议保留) */
    SSL_CTX_set_verify(g_client_ctx, SSL_VERIFY_PEER, NULL);
    /* 设置验证深度 */
    SSL_CTX_set_verify_depth(g_client_ctx, 4);

    printf("[TLS] 客户端初始化成功\n");
    return 0;
}

int tls_client_connect(SSL** ssl_out, const char* host, uint16_t port)
{
    if (!ssl_out || !host) {
        set_error("tls_client_connect: 参数为空");
        return -1;
    }

    *ssl_out = NULL;

    /* 延迟初始化: 如果客户端 TLS 未初始化, 自动初始化 */
    if (!g_client_ctx) {
        if (tls_client_init(NULL) != 0) {
            return -1;
        }
    }

    /* 创建 socket */
    int fd = (int)socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0) {
        set_error("socket() 创建失败");
        return -1;
    }

    /* DNS 解析 */
    struct hostent* he = gethostbyname(host);
    if (!he) {
        set_error("gethostbyname() 解析失败");
        close_socket(fd);
        return -1;
    }

    /* 连接 */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        set_error("connect() 连接失败");
        close_socket(fd);
        return -1;
    }

    /* 创建 SSL 对象 */
    SSL* ssl = SSL_new(g_client_ctx);
    if (!ssl) {
        set_error_openssl("SSL_new(客户端) 失败");
        close_socket(fd);
        return -1;
    }

    SSL_set_fd(ssl, fd);

    /* 设置 SNI (Server Name Indication) */
    SSL_set_tlsext_host_name(ssl, host);

    /* TLS 握手 */
    int ret = SSL_connect(ssl);
    if (ret <= 0) {
        int err = SSL_get_error(ssl, ret);
        char info[128];
        ERR_error_string_n(ERR_get_error(), info, sizeof(info));
        fprintf(stderr, "[TLS] SSL_connect 失败 (host=%s:%d): err=%d (%s)\n",
                host, port, err, info);
        SSL_free(ssl);
        close_socket(fd);
        return -1;
    }

    /* 验证服务端证书 (标准 HTTPS 验证) */
    long verify_result = SSL_get_verify_result(ssl);
    if (verify_result != X509_V_OK) {
        fprintf(stderr, "[TLS] 警告: 服务端证书验证失败: %ld (%s)\n",
                verify_result, X509_verify_cert_error_string(verify_result));
        /* 非致命: 允许继续, 但打印警告 */
    }

    char info[128];
    tls_get_info(ssl, info, sizeof(info));
    printf("[TLS] HTTPS 连接成功: %s:%d %s\n", host, port, info);

    *ssl_out = ssl;
    return fd;
}

void tls_client_cleanup(void)
{
    if (g_client_ctx) {
        SSL_CTX_free(g_client_ctx);
        g_client_ctx = NULL;
    }
}

/* ============================================================
 * 自动证书生成
 * ============================================================ */

/**
 * 使用 OpenSSL API 生成自签名 RSA 2048 证书
 */
static int generate_self_signed_cert(const char* cert_path, const char* key_path)
{
    /* 确保目录存在 */
    char dir[512];
    strncpy(dir, cert_path, sizeof(dir) - 1);
    dir[sizeof(dir) - 1] = '\0';
    char* last_slash = strrchr(dir, '/');
    if (!last_slash) last_slash = strrchr(dir, '\\');
    if (last_slash) {
        *last_slash = '\0';
#ifdef PLATFORM_WINDOWS
        char mkdir_cmd[1024];
        snprintf(mkdir_cmd, sizeof(mkdir_cmd), "if not exist \"%s\" mkdir \"%s\"", dir, dir);
        system(mkdir_cmd);
#else
        char mkdir_cmd[1024];
        snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p \"%s\"", dir);
        system(mkdir_cmd);
#endif
    }

    /* 生成 RSA 私钥 */
    EVP_PKEY* pkey = EVP_PKEY_new();
    if (!pkey) {
        set_error_openssl("EVP_PKEY_new 失败");
        return -1;
    }

    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL);
    if (!ctx) {
        set_error_openssl("EVP_PKEY_CTX_new 失败");
        EVP_PKEY_free(pkey);
        return -1;
    }

    if (EVP_PKEY_keygen_init(ctx) <= 0 ||
        EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, 2048) <= 0 ||
        EVP_PKEY_keygen(ctx, &pkey) <= 0) {
        set_error_openssl("RSA 密钥生成失败");
        EVP_PKEY_CTX_free(ctx);
        EVP_PKEY_free(pkey);
        return -1;
    }
    EVP_PKEY_CTX_free(ctx);

    /* 创建 X509 证书 */
    X509* x509 = X509_new();
    if (!x509) {
        set_error_openssl("X509_new 失败");
        EVP_PKEY_free(pkey);
        return -1;
    }

    /* 设置序列号 */
    ASN1_INTEGER_set(X509_get_serialNumber(x509), 1);

    /* 设置有效期: 现在 ~ 10年后 */
    X509_gmtime_adj(X509_get_notBefore(x509), 0);
    X509_gmtime_adj(X509_get_notAfter(x509), 365 * 24 * 3600 * 10L);

    /* 设置公钥 */
    X509_set_pubkey(x509, pkey);

    /* 设置主题名称 (自签名: issuer = subject) */
    X509_NAME* name = X509_get_subject_name(x509);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                               (unsigned char*)"127.0.0.1", -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "O", MBSTRING_ASC,
                               (unsigned char*)"Chrono-shift Self-Signed", -1, -1, 0);
    X509_set_issuer_name(x509, name);

    /* 添加 SAN (Subject Alternative Name): IP:127.0.0.1 */
    GENERAL_NAMES* sans = sk_GENERAL_NAME_new_null();
    if (sans) {
        GENERAL_NAME* san_ip = GENERAL_NAME_new();
        if (san_ip) {
            ASN1_OCTET_STRING* ip_octet = ASN1_OCTET_STRING_new();
            if (ip_octet) {
                unsigned char ip_bytes[4] = {127, 0, 0, 1};
                ASN1_OCTET_STRING_set(ip_octet, ip_bytes, 4);
                GENERAL_NAME_set0_value(san_ip, GEN_IPADD, ip_octet);
                sk_GENERAL_NAME_push(sans, san_ip);
            }
        }
        X509_add1_ext_i2d(x509, NID_subject_alt_name, sans, 0, 0);
        sk_GENERAL_NAME_pop_free(sans, GENERAL_NAME_free);
    }

    /* 自签名 */
    if (!X509_sign(x509, pkey, EVP_sha256())) {
        set_error_openssl("X509_sign 失败");
        X509_free(x509);
        EVP_PKEY_free(pkey);
        return -1;
    }

    /* 保存私钥到 PEM 文件 */
    FILE* key_file = fopen(key_path, "wb");
    if (!key_file) {
        set_error("无法写入私钥文件");
        X509_free(x509);
        EVP_PKEY_free(pkey);
        return -1;
    }
    PEM_write_PrivateKey(key_file, pkey, NULL, NULL, 0, NULL, NULL);
    fclose(key_file);

    /* 保存证书到 PEM 文件 */
    FILE* cert_file = fopen(cert_path, "wb");
    if (!cert_file) {
        set_error("无法写入证书文件");
        X509_free(x509);
        EVP_PKEY_free(pkey);
        fclose(key_file);
        return -1;
    }
    PEM_write_X509(cert_file, x509);
    fclose(cert_file);

    printf("[TLS] 自签名证书已生成: cert=%s, key=%s\n", cert_path, key_path);

    X509_free(x509);
    EVP_PKEY_free(pkey);
    return 0;
}

int tls_server_auto_init(const char* cert_dir)
{
    char cert_path[1024];
    char key_path[1024];

    snprintf(cert_path, sizeof(cert_path), "%s/server.crt", cert_dir);
    snprintf(key_path, sizeof(key_path), "%s/server.key", cert_dir);

    /* 检查证书文件是否存在 */
    FILE* f_cert = fopen(cert_path, "r");
    FILE* f_key  = fopen(key_path, "r");

    if (f_cert && f_key) {
        fclose(f_cert);
        fclose(f_key);
        printf("[TLS] 检测到现有证书: %s, %s\n", cert_path, key_path);
        return tls_server_init(cert_path, key_path);
    }

    if (f_cert) fclose(f_cert);
    if (f_key) fclose(f_key);

    /* 证书不存在，自动生成 */
    printf("[TLS] 未找到现有证书，正在生成自签名证书...\n");

    if (generate_self_signed_cert(cert_path, key_path) != 0) {
        fprintf(stderr, "[TLS] 自签名证书生成失败: %s\n", tls_last_error());
        return -1;
    }

    return tls_server_init(cert_path, key_path);
}

#endif /* HAS_TLS */
