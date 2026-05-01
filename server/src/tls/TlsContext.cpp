/**
 * Chrono-shift C++ TLS 上下文实现
 * RAII 封装 OpenSSL
 * C++17 重构版
 */
#include "TlsContext.h"
#include "../util/Logger.h"

#include <cstring>
#include <mutex>

// OpenSSL 头文件
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4996)
#endif
#include <openssl/ssl.h>
#include <openssl/err.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

namespace chrono {
namespace tls {

// 全局初始化标志
static std::once_flag s_ssl_init_flag;
static int s_ssl_init_count = 0;
static std::mutex s_init_mutex;

// ============================================================
// 构造函数/析构函数
// ============================================================
TlsContext::TlsContext(const std::string& cert_path, const std::string& key_path)
    : ctx_(nullptr)
    , cert_path_(cert_path)
    , key_path_(key_path)
{
}

TlsContext::~TlsContext()
{
    if (ctx_) {
        SSL_CTX_free(static_cast<SSL_CTX*>(ctx_));
        ctx_ = nullptr;
    }
}

// 移动构造
TlsContext::TlsContext(TlsContext&& other) noexcept
    : ctx_(other.ctx_)
    , cert_path_(std::move(other.cert_path_))
    , key_path_(std::move(other.key_path_))
    , error_(std::move(other.error_))
{
    other.ctx_ = nullptr;
}

// 移动赋值
TlsContext& TlsContext::operator=(TlsContext&& other) noexcept
{
    if (this != &other) {
        if (ctx_) {
            SSL_CTX_free(static_cast<SSL_CTX*>(ctx_));
        }
        ctx_ = other.ctx_;
        cert_path_ = std::move(other.cert_path_);
        key_path_ = std::move(other.key_path_);
        error_ = std::move(other.error_);
        other.ctx_ = nullptr;
    }
    return *this;
}

// ============================================================
// 初始化
// ============================================================
bool TlsContext::init()
{
    // 确保全局初始化
    global_init();

    // 创建 SSL_CTX
    const SSL_METHOD* method = TLS_server_method();
    if (!method) {
        error_ = "Failed to get TLS server method";
        LOG_ERROR("[TLS] %s", error_.c_str());
        return false;
    }

    SSL_CTX* ctx = SSL_CTX_new(method);
    if (!ctx) {
        error_ = get_openssl_error();
        LOG_ERROR("[TLS] Failed to create SSL_CTX: %s", error_.c_str());
        return false;
    }
    ctx_ = ctx;

    // 设置证书链
    if (SSL_CTX_use_certificate_chain_file(ctx, cert_path_.c_str()) <= 0) {
        error_ = get_openssl_error();
        LOG_ERROR("[TLS] Failed to load cert file '%s': %s",
                  cert_path_.c_str(), error_.c_str());
        SSL_CTX_free(ctx);
        ctx_ = nullptr;
        return false;
    }

    // 设置私钥
    if (SSL_CTX_use_PrivateKey_file(ctx, key_path_.c_str(), SSL_FILETYPE_PEM) <= 0) {
        error_ = get_openssl_error();
        LOG_ERROR("[TLS] Failed to load key file '%s': %s",
                  key_path_.c_str(), error_.c_str());
        SSL_CTX_free(ctx);
        ctx_ = nullptr;
        return false;
    }

    // 验证私钥
    if (!SSL_CTX_check_private_key(ctx)) {
        error_ = "Private key does not match certificate";
        LOG_ERROR("[TLS] %s", error_.c_str());
        SSL_CTX_free(ctx);
        ctx_ = nullptr;
        return false;
    }

    // 安全配置
    // 仅允许 TLS 1.2 和 1.3
    SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);
    SSL_CTX_set_max_proto_version(ctx, TLS1_3_VERSION);

    // 设置密码套件
    const char* ciphers =
        "ECDHE-ECDSA-AES128-GCM-SHA256:"
        "ECDHE-RSA-AES128-GCM-SHA256:"
        "ECDHE-ECDSA-AES256-GCM-SHA384:"
        "ECDHE-RSA-AES256-GCM-SHA384:"
        "DHE-RSA-AES128-GCM-SHA256";
    if (SSL_CTX_set_cipher_list(ctx, ciphers) <= 0) {
        LOG_WARN("[TLS] Failed to set cipher list, using defaults");
    }

    // TLS 1.3 密码套件
    const char* ciphersuites =
        "TLS_AES_128_GCM_SHA256:"
        "TLS_AES_256_GCM_SHA384:"
        "TLS_CHACHA20_POLY1305_SHA256";
    if (SSL_CTX_set_ciphersuites(ctx, ciphersuites) <= 0) {
        LOG_WARN("[TLS] Failed to set TLS 1.3 ciphersuites, using defaults");
    }

    // 启用 session cache
    SSL_CTX_set_session_cache_mode(ctx, SSL_SESS_CACHE_SERVER);

    // 设置 DH 参数 (自动)
    SSL_CTX_set_dh_auto(ctx, 1);

    // 设置 ECDH 曲线
    SSL_CTX_set_ecdh_auto(ctx, 1);

    LOG_INFO("[TLS] TLS context initialized (cert=%s)", cert_path_.c_str());
    return true;
}

// ============================================================
// SSL 操作
// ============================================================
void* TlsContext::new_ssl(int fd)
{
    if (!ctx_) {
        LOG_ERROR("[TLS] new_ssl called but context not initialized");
        return nullptr;
    }

    SSL* ssl = SSL_new(static_cast<SSL_CTX*>(ctx_));
    if (!ssl) {
        error_ = get_openssl_error();
        LOG_ERROR("[TLS] SSL_new failed: %s", error_.c_str());
        return nullptr;
    }

    SSL_set_fd(ssl, fd);
    SSL_set_accept_state(ssl);
    return ssl;
}

int TlsContext::accept(void* ssl_ptr)
{
    SSL* ssl = static_cast<SSL*>(ssl_ptr);
    int ret = SSL_accept(ssl);
    if (ret == 1) {
        return 0; // 成功
    }
    int err = SSL_get_error(ssl, ret);
    if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
        return 1; // 需要重试
    }
    error_ = get_openssl_error();
    return -1; // 错误
}

int TlsContext::read(void* ssl_ptr, uint8_t* buf, int size)
{
    SSL* ssl = static_cast<SSL*>(ssl_ptr);
    int ret = SSL_read(ssl, buf, size);
    if (ret > 0) {
        return ret;
    }
    int err = SSL_get_error(ssl, ret);
    if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
        return 0; // 需要重试
    }
    return -1; // 连接关闭或错误
}

int TlsContext::write(void* ssl_ptr, const uint8_t* buf, int size)
{
    SSL* ssl = static_cast<SSL*>(ssl_ptr);
    int ret = SSL_write(ssl, buf, size);
    if (ret > 0) {
        return ret;
    }
    int err = SSL_get_error(ssl, ret);
    if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
        return 0; // 需要重试
    }
    return -1; // 错误
}

void TlsContext::shutdown(void* ssl_ptr)
{
    if (!ssl_ptr) return;
    SSL* ssl = static_cast<SSL*>(ssl_ptr);
    SSL_shutdown(ssl);
}

void TlsContext::free_ssl(void* ssl_ptr)
{
    if (!ssl_ptr) return;
    SSL* ssl = static_cast<SSL*>(ssl_ptr);
    SSL_free(ssl);
}

std::string TlsContext::last_error() const
{
    return error_;
}

// ============================================================
// 错误处理
// ============================================================
std::string TlsContext::get_openssl_error()
{
    unsigned long err = ERR_get_error();
    if (err == 0) {
        return "No error";
    }
    char buf[256];
    ERR_error_string_n(err, buf, sizeof(buf));
    return std::string(buf);
}

// ============================================================
// 全局初始化/清理
// ============================================================
bool TlsContext::global_init()
{
    std::call_once(s_ssl_init_flag, []() {
        SSL_library_init();
        SSL_load_error_strings();
        OpenSSL_add_all_algorithms();
    });
    return true;
}

void TlsContext::global_cleanup()
{
    // OpenSSL 没有线程安全的完全清理，但在大多数平台上可以安全调用
    EVP_cleanup();
    ERR_free_strings();
}

} // namespace tls
} // namespace chrono
