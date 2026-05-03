/**
 * Chrono-shift 客户端 — 服务端 TLS 上下文实现
 *
 * 基于 OpenSSL，为 ClientHttpServer 提供 HTTPS 能力
 *
 * 依赖:
 *   client/certs/server.crt — 自签名证书
 *   client/certs/server.key — 私钥
 *
 * OpenSSL 初始化:
 *   TlsServerContext 内部使用静态标志确保 OpenSSL 全局初始化仅执行一次，
 *   不影响已有 TlsWrapper/tls_client.c 的使用。
 */

#include "TlsServerContext.h"

#if HTTPS_SUPPORT
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif

#include <cstring>

namespace chrono {
namespace client {
namespace app {

// ============================================================
// 内部：OpenSSL 全局初始化 (线程安全，仅执行一次)
// ============================================================
namespace {
    bool g_openssl_inited = false;

    void ensure_openssl_init()
    {
        if (!g_openssl_inited) {
            SSL_load_error_strings();
            OpenSSL_add_ssl_algorithms();
            g_openssl_inited = true;
        }
    }
} // anonymous namespace

// ============================================================
// 构造 / 析构
// ============================================================

TlsServerContext::TlsServerContext(const std::string& cert_file,
                                   const std::string& key_file)
    : ctx_(nullptr)
{
    ensure_openssl_init();

    // 创建服务端 SSL_CTX (TLS 1.2+)
    ctx_ = SSL_CTX_new(TLS_server_method());
    if (!ctx_) {
        last_error_ = "SSL_CTX_new(TLS_server_method) 失败";
        return;
    }

    // 仅允许 TLS 1.2 及以上版本 (禁用 SSLv2/v3, TLS 1.0/1.1)
    SSL_CTX_set_min_proto_version(ctx_, TLS1_2_VERSION);

    // 加载 PEM 证书
    if (SSL_CTX_use_certificate_file(ctx_, cert_file.c_str(),
                                     SSL_FILETYPE_PEM) <= 0) {
        last_error_ = "加载证书失败: " + cert_file;
        ERR_clear_error();
        SSL_CTX_free(ctx_);
        ctx_ = nullptr;
        return;
    }

    // 加载 PEM 私钥
    if (SSL_CTX_use_PrivateKey_file(ctx_, key_file.c_str(),
                                    SSL_FILETYPE_PEM) <= 0) {
        last_error_ = "加载私钥失败: " + key_file;
        ERR_clear_error();
        SSL_CTX_free(ctx_);
        ctx_ = nullptr;
        return;
    }

    // 验证证书与私钥是否匹配
    if (!SSL_CTX_check_private_key(ctx_)) {
        last_error_ = "私钥与证书不匹配";
        ERR_clear_error();
        SSL_CTX_free(ctx_);
        ctx_ = nullptr;
        return;
    }
}

TlsServerContext::~TlsServerContext()
{
    if (ctx_) {
        SSL_CTX_free(ctx_);
        ctx_ = nullptr;
    }
}

// ============================================================
// 移动语义
// ============================================================

TlsServerContext::TlsServerContext(TlsServerContext&& other) noexcept
    : ctx_(other.ctx_)
    , last_error_(std::move(other.last_error_))
{
    other.ctx_ = nullptr;
}

TlsServerContext& TlsServerContext::operator=(TlsServerContext&& other) noexcept
{
    if (this != &other) {
        if (ctx_) {
            SSL_CTX_free(ctx_);
        }
        ctx_ = other.ctx_;
        last_error_ = std::move(other.last_error_);
        other.ctx_ = nullptr;
    }
    return *this;
}

// ============================================================
// TLS 握手
// ============================================================

struct ssl_st* TlsServerContext::accept(int fd)
{
    if (!ctx_) {
        last_error_ = "TLS 上下文未初始化";
        return nullptr;
    }

    SSL* ssl = SSL_new(ctx_);
    if (!ssl) {
        last_error_ = "SSL_new 失败";
        return nullptr;
    }

    SSL_set_fd(ssl, fd);

    int ret = SSL_accept(ssl);
    if (ret <= 0) {
        unsigned long err_code = ERR_get_error();
        if (err_code != 0) {
            char err_buf[256];
            ERR_error_string_n(err_code, err_buf, sizeof(err_buf));
            last_error_ = err_buf;
        } else {
            // ret == 0 表示 TLS 握手被对端关闭
            // ret < 0 表示发生非致命的 I/O 错误
            last_error_ = (ret == 0)
                ? "TLS 握手被客户端关闭"
                : "TLS 握手失败 (非致命 I/O 错误)";
        }
        SSL_free(ssl);
        return nullptr;
    }

    return ssl;
}

// ============================================================
// 错误信息
// ============================================================

const char* TlsServerContext::last_error() const
{
    return last_error_.c_str();
}

} // namespace app
} // namespace client
} // namespace chrono
