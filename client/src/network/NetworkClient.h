/**
 * Chrono-shift 客户端网络模块 — 总 Facade
 * C++17 重构版
 *
 * 统一管理 TCP/TLS 连接、HTTP 请求、WebSocket 通信
 * 自动处理重连、认证令牌
 */
#ifndef CHRONO_CLIENT_NETWORK_CLIENT_H
#define CHRONO_CLIENT_NETWORK_CLIENT_H

#include <cstdint>
#include <string>
#include <memory>

#include "TcpConnection.h"
#include "HttpConnection.h"
#include "WebSocketClient.h"

namespace chrono {
namespace client {
namespace network {

/**
 * 网络客户端 Facade
 *
 * 包装 TcpConnection + HttpConnection + WebSocketClient 为一站式接口
 * 负责连接生命周期、自动重连、认证令牌管理
 *
 * 使用示例:
 * @code
 *   NetworkClient client;
 *   if (client.connect("example.com", 443, true)) {
 *       auto resp = client.http_request("GET", "/api/status");
 *       if (resp.status_code == 200) { ... }
 *       client.ws_handshake("/ws");
 *       client.ws_send(WebSocketClient::WsOpcode::kText, ...);
 *   }
 * @endcode
 */
class NetworkClient {
public:
    /**
     * 构造函数
     * 不会自动初始化 Winsock；首次 connect() 时会初始化
     */
    NetworkClient();

    /** 析构函数 — 自动断开连接并清理 Winsock */
    ~NetworkClient();

    /* 禁止拷贝，允许移动 */
    NetworkClient(const NetworkClient&) = delete;
    NetworkClient& operator=(const NetworkClient&) = delete;
    NetworkClient(NetworkClient&&) = default;
    NetworkClient& operator=(NetworkClient&&) = default;

    // ============================================================
    // 连接管理
    // ============================================================

    /**
     * 连接到服务器
     * @param host   服务器主机名或 IP
     * @param port   服务器端口
     * @param use_tls 是否启用 TLS
     * @return true=连接成功
     */
    bool connect(const std::string& host, uint16_t port, bool use_tls = false);

    /**
     * 断开连接
     */
    void disconnect();

    /**
     * 是否已连接
     */
    bool is_connected() const;

    /**
     * 设置认证令牌 (自动附加到 HTTP 请求头)
     */
    void set_auth_token(const std::string& token);

    /**
     * 获取认证令牌
     */
    const std::string& get_auth_token() const;

    /**
     * 设置自动重连
     * @param enable      启用/禁用
     * @param max_retries 最大重试次数 (-1 为无限)
     */
    void set_auto_reconnect(bool enable, int max_retries = -1);

    /**
     * 手动触发重连
     * @return true=重连成功
     */
    bool reconnect();

    /**
     * 获取底层 TcpConnection 引用 (高级操作)
     */
    TcpConnection& get_tcp();

    // ============================================================
    // HTTP API
    // ============================================================

    /**
     * 发起 HTTP 请求 (带自动重连重试)
     * @param method   HTTP 方法 (GET/POST/PUT/DELETE 等)
     * @param path     请求路径 (如 "/api/users")
     * @param headers  额外请求头 (每行一个，不含 Host/Content-Length)
     * @param body     请求体数据 (可为 nullptr)
     * @param body_len 请求体长度
     * @return HTTP 响应 (含状态码、头、体)
     */
    HttpConnection::Response http_request(
        const std::string& method,
        const std::string& path,
        const std::string& headers = "",
        const uint8_t* body = nullptr,
        size_t body_len = 0);

    // ============================================================
    // WebSocket API
    // ============================================================

    /**
     * WebSocket 握手
     * @param path 请求路径 (如 "/ws")
     * @return true=握手成功
     */
    bool ws_handshake(const std::string& path);

    /**
     * 发送 WebSocket 帧
     * @param opcode  帧类型
     * @param payload 负载数据
     * @param len     负载长度
     * @return 0=成功, -1=失败
     */
    int ws_send(WebSocketClient::WsOpcode opcode,
                const uint8_t* payload, size_t len);

    /**
     * 接收 WebSocket 帧
     * @param frame 输出: 接收到的帧
     * @return 0=成功, -1=失败
     */
    int ws_recv(WebSocketClient::WsFrame& frame);

    /**
     * WebSocket 是否已连接
     */
    bool ws_is_connected() const;

private:
    /** 底层 TCP/TLS 连接 */
    TcpConnection tcp_;

    /** HTTP 请求封装 (引用 tcp_) */
    std::unique_ptr<HttpConnection> http_;

    /** WebSocket 客户端 */
    WebSocketClient ws_;

    /** 认证令牌 */
    std::string auth_token_;

    /** 服务器信息 (用于重连) */
    std::string server_host_;
    uint16_t server_port_ = 0;
    bool use_tls_ = false;

    /** 是否已初始化过 Winsock (全局，类级) */
    static int s_winsock_refcount_;
};

} // namespace network
} // namespace client
} // namespace chrono

#endif // CHRONO_CLIENT_NETWORK_CLIENT_H
