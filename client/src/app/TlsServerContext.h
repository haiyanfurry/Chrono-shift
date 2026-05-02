/**
 * Chrono-shift 客户端 — 服务端 TLS 上下文 RAII 封装
 *
 * 为 ClientHttpServer 提供 HTTPS 能力
 * 加载自签名证书，对每个客户端连接执行 SSL_accept
 *
 * 依赖 OpenSSL 1.1+ / 3.x
 */

#ifndef CHRONO_CLIENT_TLS_SERVER_CONTEXT_H
#define CHRONO_CLIENT_TLS_SERVER_CONTEXT_H

#include <string>
#include <memory>

struct ssl_st;      // SSL*
struct ssl_ctx_st;  // SSL_CTX*

namespace chrono {
namespace client {
namespace app {

/**
 * 服务端 TLS 上下文
 *
 * RAII 封装，管理 OpenSSL 的 SSL_CTX 生命周期
 * 加载 PEM 格式的证书和私钥，在已 accept 的 socket 上执行 TLS 握手
 *
 * 使用示例:
 * @code
 *   TlsServerContext tls("certs/server.crt", "certs/server.key");
 *   if (!tls.is_valid()) { /* 处理错误 */ }
 *
 *   // 对每个客户端连接:
 *   SSL* ssl = tls.accept(client_fd);
 *   if (ssl) {
 *       SSL_write(ssl, response, len);
 *       SSL_free(ssl);
 *   }
 * @endcode
 */
class TlsServerContext {
public:
    /**
     * 构造并初始化服务端 TLS 上下文
     * @param cert_file PEM 证书文件路径
     * @param key_file  PEM 私钥文件路径
     *
     * 构造函数中会加载证书、私钥并验证匹配。
     * 创建失败时 is_valid() 返回 false，可通过 last_error() 获取错误描述。
     */
    TlsServerContext(const std::string& cert_file,
                     const std::string& key_file);

    /** 析构：释放 SSL_CTX */
    ~TlsServerContext();

    // 禁止拷贝
    TlsServerContext(const TlsServerContext&) = delete;
    TlsServerContext& operator=(const TlsServerContext&) = delete;

    // 允许移动
    TlsServerContext(TlsServerContext&& other) noexcept;
    TlsServerContext& operator=(TlsServerContext&& other) noexcept;

    /**
     * 检查上下文是否初始化成功
     * @return true 证书和密钥已成功加载，可以接受 TLS 连接
     */
    bool is_valid() const { return ctx_ != nullptr; }

    /**
     * 在已接受的 socket 上执行 TLS 握手
     * @param fd 已通过 accept() 获取的 socket 描述符
     * @return SSL* 对象指针，失败返回 nullptr
     *
     * 调用者负责通过 SSL_free() 释放返回的 SSL* 对象。
     * 同时调用者也负责关闭 fd。
     */
    struct ssl_st* accept(int fd);

    /**
     * 获取最后错误描述
     * @return 错误字符串，无错误时返回空字符串
     */
    const char* last_error() const;

private:
    struct ssl_ctx_st* ctx_;       ///< OpenSSL 服务端 CTX
    std::string last_error_;       ///< 最后错误描述缓存
};

} // namespace app
} // namespace client
} // namespace chrono

#endif // CHRONO_CLIENT_TLS_SERVER_CONTEXT_H
