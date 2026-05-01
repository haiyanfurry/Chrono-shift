/**
 * Chrono-shift C++ TLS 上下文
 * RAII 封装 OpenSSL/mbedTLS
 * C++17 重构版
 */
#ifndef CHRONO_CPP_TLS_CONTEXT_H
#define CHRONO_CPP_TLS_CONTEXT_H

#include <string>
#include <memory>
#include <cstdint>
#include <vector>

namespace chrono {
namespace tls {

/**
 * TLS 上下文 — RAII 管理 SSL_CTX*
 * 自动初始化和清理 OpenSSL
 */
class TlsContext {
public:
    /**
     * @param cert_path 证书文件路径 (PEM)
     * @param key_path  私钥文件路径 (PEM)
     */
    TlsContext(const std::string& cert_path, const std::string& key_path);
    ~TlsContext();

    // 禁止拷贝
    TlsContext(const TlsContext&) = delete;
    TlsContext& operator=(const TlsContext&) = delete;

    // 允许移动
    TlsContext(TlsContext&& other) noexcept;
    TlsContext& operator=(TlsContext&& other) noexcept;

    /**
     * 初始化 TLS 上下文
     * @return true 如果成功
     */
    bool init();

    /**
     * 是否已初始化
     */
    bool is_initialized() const { return ctx_ != nullptr; }

    /**
     * 创建新 SSL 连接
     * @param fd socket 文件描述符
     * @return SSL 对象指针，失败返回 nullptr
     */
    void* new_ssl(int fd);

    /**
     * 接受 TLS 握手
     * @param ssl SSL 对象指针
     * @return 0=成功, <0=错误, >0=需要重试
     */
    int accept(void* ssl);

    /**
     * 读取数据
     * @return 读取的字节数, <=0 表示错误
     */
    int read(void* ssl, uint8_t* buf, int size);

    /**
     * 写入数据
     * @return 写入的字节数, <=0 表示错误
     */
    int write(void* ssl, const uint8_t* buf, int size);

    /**
     * 关闭 SSL 连接
     */
    void shutdown(void* ssl);

    /**
     * 释放 SSL 对象
     */
    void free_ssl(void* ssl);

    /**
     * 获取错误信息
     */
    std::string last_error() const;

    /**
     * 全局初始化/清理 (自动调用)
     */
    static bool global_init();
    static void global_cleanup();

private:
    /** 获取 OpenSSL 错误信息 */
    std::string get_openssl_error();

    void* ctx_ = nullptr;       // SSL_CTX*
    std::string cert_path_;
    std::string key_path_;
    std::string error_;
};

} // namespace tls
} // namespace chrono

#endif // CHRONO_CPP_TLS_CONTEXT_H
