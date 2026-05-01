/**
 * Chrono-shift 客户端网络模块 — 总 Facade 实现
 * C++17 重构版
 *
 * 统一管理 TCP/TLS 连接、HTTP 请求、WebSocket 通信
 */

#include "NetworkClient.h"

#include <cstring>

#include <winsock2.h>
#include <windows.h>

#include "../util/Logger.h"

namespace chrono {
namespace client {
namespace network {

/* 全局 Winsock 引用计数 */
int NetworkClient::s_winsock_refcount_ = 0;

/* ============================================================
 * 构造函数 / 析构函数
 * ============================================================ */

NetworkClient::NetworkClient()
    : server_port_(0)
    , use_tls_(false)
{
}

NetworkClient::~NetworkClient()
{
    disconnect();
}

/* ============================================================
 * 连接管理
 * ============================================================ */

bool NetworkClient::connect(const std::string& host, uint16_t port, bool use_tls)
{
    /* 记录连接参数 (用于重连) */
    server_host_ = host;
    server_port_ = port;
    use_tls_     = use_tls;

    /* 首次连接时初始化 Winsock */
    if (s_winsock_refcount_ == 0) {
        if (!TcpConnection::init_winsock()) {
            LOG_ERROR("NetworkClient: Winsock 初始化失败");
            return false;
        }
    }
    s_winsock_refcount_++;

    /* 建立 TCP/TLS 连接 */
    if (!tcp_.connect(host, port, use_tls)) {
        LOG_ERROR("NetworkClient: 连接 %s:%u 失败", host.c_str(), port);
        s_winsock_refcount_--;
        if (s_winsock_refcount_ == 0) {
            TcpConnection::cleanup_winsock();
        }
        return false;
    }

    /* 创建 HTTP 封装 */
    http_ = std::make_unique<HttpConnection>(tcp_);

    LOG_INFO("NetworkClient: 已连接到 %s:%u%s",
             host.c_str(), port, use_tls ? " (TLS)" : "");
    return true;
}

void NetworkClient::disconnect()
{
    tcp_.disconnect();
    http_.reset();

    if (s_winsock_refcount_ > 0) {
        s_winsock_refcount_--;
        if (s_winsock_refcount_ == 0) {
            TcpConnection::cleanup_winsock();
        }
    }

    LOG_DEBUG("NetworkClient: 已断开连接");
}

bool NetworkClient::is_connected() const
{
    return tcp_.is_connected();
}

void NetworkClient::set_auth_token(const std::string& token)
{
    auth_token_ = token;
}

const std::string& NetworkClient::get_auth_token() const
{
    return auth_token_;
}

void NetworkClient::set_auto_reconnect(bool enable, int max_retries)
{
    tcp_.set_auto_reconnect(enable, max_retries);
}

bool NetworkClient::reconnect()
{
    if (server_host_.empty()) {
        LOG_ERROR("NetworkClient: 无服务器信息，无法重连");
        return false;
    }

    LOG_INFO("NetworkClient: 正在重连 %s:%u...",
             server_host_.c_str(), server_port_);

    tcp_.disconnect();
    http_.reset();

    if (!tcp_.connect(server_host_, server_port_, use_tls_)) {
        LOG_ERROR("NetworkClient: 重连失败");
        return false;
    }

    http_ = std::make_unique<HttpConnection>(tcp_);
    LOG_INFO("NetworkClient: 重连成功");
    return true;
}

TcpConnection& NetworkClient::get_tcp()
{
    return tcp_;
}

/* ============================================================
 * HTTP API
 * ============================================================ */

HttpConnection::Response NetworkClient::http_request(
    const std::string& method,
    const std::string& path,
    const std::string& headers,
    const uint8_t* body,
    size_t body_len)
{
    /* 如果未连接，尝试自动重连 */
    if (!tcp_.is_connected()) {
        LOG_WARN("NetworkClient: 连接已断开，尝试重连...");
        if (!reconnect()) {
            LOG_ERROR("NetworkClient: HTTP 请求失败 — 无法重连");
            HttpConnection::Response err_resp;
            err_resp.status_code = 0;
            err_resp.status_text = "Connection Lost";
            return err_resp;
        }
    }

    /* 构建完整头部 (附加认证令牌) */
    std::string full_headers;
    if (!auth_token_.empty()) {
        full_headers = "Authorization: Bearer " + auth_token_ + "\r\n";
    }
    if (!headers.empty()) {
        full_headers += headers;
    }

    /* 发起请求 (HttpConnection 内部已有重试逻辑) */
    return http_->request(method, path, full_headers, body, body_len);
}

/* ============================================================
 * WebSocket API
 * ============================================================ */

bool NetworkClient::ws_handshake(const std::string& path)
{
    if (!tcp_.is_connected()) {
        LOG_ERROR("NetworkClient: WebSocket 握手失败 — 未连接");
        return false;
    }

    /* 构造 Host 头 (用于握手) */
    std::string host = server_host_;
    if (server_port_ != 80 && server_port_ != 443) {
        host += ":" + std::to_string(server_port_);
    }

    return ws_.handshake(tcp_, host, path);
}

int NetworkClient::ws_send(WebSocketClient::WsOpcode opcode,
                           const uint8_t* payload, size_t len)
{
    return ws_.send_frame(opcode, payload, len);
}

int NetworkClient::ws_recv(WebSocketClient::WsFrame& frame)
{
    return ws_.recv_frame(frame);
}

bool NetworkClient::ws_is_connected() const
{
    return tcp_.is_connected();
}

} // namespace network
} // namespace client
} // namespace chrono
