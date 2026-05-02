/**
 * Chrono-shift 客户端 TLS RAII 包装
 * C++17 重构版
 */
#ifndef CHRONO_CLIENT_TLS_WRAPPER_H
#define CHRONO_CLIENT_TLS_WRAPPER_H

#include <cstdint>
#include <memory>
#include <string>

/* OpenSSL 前向声明 (匹配 server/include/tls_server.h 的 opaque 类型) */
struct ssl_st;
struct ssl_ctx_st;

namespace chrono {
namespace client {
namespace network {

/**
 * TLS 连接 RAII 包装
 * 管理 SSL* 对象的生命周期，提供读/写接口
 */
class TlsConnection {
public:
    /** 接管 SSL 对象和 socket 所有权 */
    TlsConnection(int fd, struct ssl_st* ssl) noexcept;

    /** 析构: 自动关闭 TLS 和 socket */
    ~TlsConnection();

    /** 禁止拷贝 */
    TlsConnection(const TlsConnection&) = delete;
    TlsConnection& operator=(const TlsConnection&) = delete;

    /** 允许移动 */
    TlsConnection(TlsConnection&& other) noexcept;
    TlsConnection& operator=(TlsConnection&& other) noexcept;

    /** TLS 读取 */
    int read(void* buf, int len) noexcept;

    /** TLS 写入 */
    int write(const void* buf, int len) noexcept;

    /** 判断连接是否有效 */
    bool is_valid() const noexcept { return ssl_ != nullptr; }

    /** 获取底层 SSL 指针 (用于 C 接口交互) */
    struct ssl_st* native_handle() const noexcept { return ssl_; }

    /** 获取底层 socket */
    int native_socket() const noexcept { return fd_; }

private:
    void cleanup() noexcept;

    int fd_;
    struct ssl_st* ssl_;
};

/**
 * TLS 客户端上下文 RAII 包装
 * 管理 SSL_CTX* 生命周期 (单次初始化，多次连接)
 */
class TlsClientContext {
public:
    /**
     * 初始化客户端 TLS 上下文
     * @param ca_file 可选 CA 证书路径，空字符串表示使用系统信任库
     */
    explicit TlsClientContext(const std::string& ca_file = "");

    /** 析构: 自动清理 TLS 上下文 */
    ~TlsClientContext();

    /** 禁止拷贝 */
    TlsClientContext(const TlsClientContext&) = delete;
    TlsClientContext& operator=(const TlsClientContext&) = delete;

    /** 允许移动 */
    TlsClientContext(TlsClientContext&& other) noexcept;
    TlsClientContext& operator=(TlsClientContext&& other) noexcept;

    /**
     * 连接到远程 TLS 服务器
     * @param host 服务器域名
     * @param port 服务器端口
     * @return TlsConnection 对象 (通过 is_valid() 检查成功)
     */
    std::unique_ptr<TlsConnection> connect(const std::string& host, uint16_t port);

    /** 检查上下文是否有效 */
    bool is_valid() const noexcept { return initialized_; }

private:
    bool initialized_;
};

} // namespace network
} // namespace client
} // namespace chrono

#endif // CHRONO_CLIENT_TLS_WRAPPER_H
