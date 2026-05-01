/**
 * Chrono-shift 客户端 TCP/TLS 连接 RAII 封装
 * C++17 重构版
 */
#ifndef CHRONO_CLIENT_TCP_CONNECTION_H
#define CHRONO_CLIENT_TCP_CONNECTION_H

#include <cstdint>
#include <memory>
#include <string>

namespace chrono {
namespace client {
namespace network {

class TlsConnection;
class TlsClientContext;

/**
 * TCP/TLS 连接 RAII 封装
 * 支持 Winsock 引用计数、TLS、自动重连 (指数退避)
 */
class TcpConnection {
public:
    /** 重连配置常量 */
    static constexpr int kMaxReconnectDelayMs = 30000;
    static constexpr int kInitialReconnectDelayMs = 500;
    static constexpr int kReconnectBackoffFactor = 2;

    /** 默认构造（未连接状态） */
    TcpConnection();

    /** 析构：自动断开连接 */
    ~TcpConnection();

    /** 禁止拷贝 */
    TcpConnection(const TcpConnection&) = delete;
    TcpConnection& operator=(const TcpConnection&) = delete;

    /** 允许移动 */
    TcpConnection(TcpConnection&& other) noexcept;
    TcpConnection& operator=(TcpConnection&& other) noexcept;

    /**
     * 连接到服务器
     * @param host 服务器地址
     * @param port 服务器端口
     * @param use_tls 是否使用 TLS
     * @return true=成功
     */
    bool connect(const std::string& host, uint16_t port, bool use_tls = false);

    /** 断开连接 */
    void disconnect();

    /** 发送全部数据 */
    int send_all(const uint8_t* data, size_t len);

    /** 接收指定长度的数据 */
    int recv_all(uint8_t* buf, size_t len);

    /** 检查是否已连接 */
    bool is_connected() const { return connected_; }

    /** 获取底层 socket */
    int get_socket() const { return sock_; }

    /** 获取 TLS 对象指针（用于 C 接口交互） */
    void* get_ssl() const;

    /** 获取服务器主机名 */
    const std::string& host() const { return host_; }

    /** 获取服务器端口 */
    uint16_t port() const { return port_; }

    /** 是否使用 TLS */
    bool use_tls() const { return use_tls_; }

    /**
     * 设置自动重连
     * @param enable 是否启用
     * @param max_retries 最大重试次数 (-1=无限)
     */
    void set_auto_reconnect(bool enable, int max_retries = -1);

    /**
     * 执行自动重连 (指数退避)
     * @return true=重连成功
     */
    bool reconnect();

    /** 初始化 Winsock (引用计数) */
    static bool init_winsock();

    /** 清理 Winsock (引用计数) */
    static void cleanup_winsock();

private:
    void cleanup();

    int sock_;
    bool use_tls_;
    bool connected_;
    std::string host_;
    uint16_t port_;

    // TLS 相关
    std::unique_ptr<TlsClientContext> tls_ctx_;
    std::unique_ptr<TlsConnection> tls_conn_;

    // 重连状态
    int reconnect_count_;
    int max_retries_;
    bool auto_reconnect_;
};

} // namespace network
} // namespace client
} // namespace chrono

#endif // CHRONO_CLIENT_TCP_CONNECTION_H
