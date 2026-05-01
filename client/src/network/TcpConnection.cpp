/**
 * Chrono-shift 客户端 TCP/TLS 连接实现
 * C++17 重构版 (从 C99 net_tcp.c 移植)
 */
#include "TcpConnection.h"
#include "TlsWrapper.h"

#include <cstring>
#include <stdexcept>

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#endif

#include "../../server/include/tls_server.h"

#include "../util/Logger.h"

namespace chrono {
namespace client {
namespace network {

// ============================================================
// Winsock 引用计数 (全局)
// ============================================================

namespace {
    int g_winsock_count = 0;
}

bool TcpConnection::init_winsock()
{
    if (g_winsock_count == 0) {
#ifdef _WIN32
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
            LOG_ERROR("Winsock 初始化失败");
            return false;
        }
#endif
    }
    g_winsock_count++;
    return true;
}

void TcpConnection::cleanup_winsock()
{
    if (g_winsock_count > 0) {
        g_winsock_count--;
        if (g_winsock_count == 0) {
#ifdef _WIN32
            WSACleanup();
#endif
        }
    }
}

// ============================================================
// TcpConnection
// ============================================================

TcpConnection::TcpConnection()
    : sock_(-1)
    , use_tls_(false)
    , connected_(false)
    , port_(0)
    , reconnect_count_(0)
    , max_retries_(-1)
    , auto_reconnect_(true)
{
}

TcpConnection::~TcpConnection()
{
    disconnect();
}

TcpConnection::TcpConnection(TcpConnection&& other) noexcept
    : sock_(other.sock_)
    , use_tls_(other.use_tls_)
    , connected_(other.connected_)
    , host_(std::move(other.host_))
    , port_(other.port_)
    , tls_ctx_(std::move(other.tls_ctx_))
    , tls_conn_(std::move(other.tls_conn_))
    , reconnect_count_(other.reconnect_count_)
    , max_retries_(other.max_retries_)
    , auto_reconnect_(other.auto_reconnect_)
{
    other.sock_ = -1;
    other.connected_ = false;
    other.port_ = 0;
}

TcpConnection& TcpConnection::operator=(TcpConnection&& other) noexcept
{
    if (this != &other) {
        disconnect();
        sock_ = other.sock_;
        use_tls_ = other.use_tls_;
        connected_ = other.connected_;
        host_ = std::move(other.host_);
        port_ = other.port_;
        tls_ctx_ = std::move(other.tls_ctx_);
        tls_conn_ = std::move(other.tls_conn_);
        reconnect_count_ = other.reconnect_count_;
        max_retries_ = other.max_retries_;
        auto_reconnect_ = other.auto_reconnect_;
        other.sock_ = -1;
        other.connected_ = false;
        other.port_ = 0;
    }
    return *this;
}

bool TcpConnection::connect(const std::string& host, uint16_t port, bool use_tls)
{
    if (connected_) {
        disconnect();
    }

    init_winsock();
    reconnect_count_ = 0;
    use_tls_ = use_tls;

    if (use_tls) {
        // TLS 模式: 使用 tls_client_connect
        if (!tls_ctx_) {
            try {
                tls_ctx_ = std::make_unique<TlsClientContext>();
            } catch (const std::exception& e) {
                LOG_ERROR("TLS 客户端初始化失败: %s", e.what());
                return false;
            }
        }

        auto conn = tls_ctx_->connect(host, port);
        if (!conn || !conn->is_valid()) {
            LOG_ERROR("TLS 连接失败: %s:%d", host.c_str(), port);
            connected_ = false;
            return false;
        }
        tls_conn_ = std::move(conn);
        sock_ = tls_conn_->native_socket();
    } else {
        // 明文 TCP 模式
        sock_ = static_cast<int>(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
        if (sock_ < 0) {
            LOG_ERROR("socket() 失败");
            return false;
        }

        // 设置超时
#ifdef _WIN32
        int timeout = 10000; /* 10秒 */
        setsockopt(sock_, SOL_SOCKET, SO_RCVTIMEO,
                   reinterpret_cast<const char*>(&timeout), sizeof(timeout));
        setsockopt(sock_, SOL_SOCKET, SO_SNDTIMEO,
                   reinterpret_cast<const char*>(&timeout), sizeof(timeout));
#endif

        // 解析地址
        struct hostent* he = gethostbyname(host.c_str());
        if (!he) {
            LOG_ERROR("gethostbyname() 失败: %s", host.c_str());
#ifdef _WIN32
            closesocket(sock_);
#else
            ::close(sock_);
#endif
            sock_ = -1;
            return false;
        }

        struct sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        std::memcpy(&addr.sin_addr, he->h_addr_list[0], static_cast<size_t>(he->h_length));

        if (::connect(sock_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0) {
            LOG_ERROR("connect() 失败: %s:%d", host.c_str(), port);
#ifdef _WIN32
            closesocket(sock_);
#else
            ::close(sock_);
#endif
            sock_ = -1;
            return false;
        }
    }

    host_ = host;
    port_ = port;
    connected_ = true;

    LOG_INFO("已连接到服务器: %s:%d%s",
             host.c_str(), port,
             use_tls ? " (TLS)" : "");

    return true;
}

void TcpConnection::disconnect()
{
    if (connected_) {
        LOG_DEBUG("断开连接: %s:%d", host_.c_str(), port_);
    }
    cleanup();
}

void TcpConnection::cleanup()
{
    tls_conn_.reset();
    tls_ctx_.reset();

    if (sock_ >= 0) {
#ifdef _WIN32
        closesocket(sock_);
#else
        ::close(sock_);
#endif
        sock_ = -1;
    }

    connected_ = false;
    cleanup_winsock();
}

int TcpConnection::send_all(const uint8_t* data, size_t len)
{
    if (!connected_ || !data) return -1;

    size_t total_sent = 0;
    while (total_sent < len) {
        int n;
        if (use_tls_ && tls_conn_) {
            n = tls_conn_->write(data + total_sent,
                                 static_cast<int>(len - total_sent));
        } else {
#ifdef _WIN32
            n = send(sock_,
                     reinterpret_cast<const char*>(data + total_sent),
                       static_cast<int>(len - total_sent), 0);
#else
            n = static_cast<int>(::write(sock_, data + total_sent, len - total_sent));
#endif
        }

        if (n <= 0) {
            LOG_ERROR("send_all 失败: 已发送 %zu/%zu 字节", total_sent, len);
            connected_ = false;
            return -1;
        }
        total_sent += static_cast<size_t>(n);
    }

    return 0;
}

int TcpConnection::recv_all(uint8_t* buf, size_t len)
{
    if (!connected_ || !buf) return -1;

    size_t total_recv = 0;
    while (total_recv < len) {
        int n;
        if (use_tls_ && tls_conn_) {
            n = tls_conn_->read(buf + total_recv,
                                static_cast<int>(len - total_recv));
        } else {
#ifdef _WIN32
            n = recv(sock_,
                     reinterpret_cast<char*>(buf + total_recv),
                       static_cast<int>(len - total_recv), 0);
#else
            n = static_cast<int>(::read(sock_, buf + total_recv, len - total_recv));
#endif
        }

        if (n <= 0) {
            LOG_ERROR("recv_all 失败: 已接收 %zu/%zu 字节", total_recv, len);
            connected_ = false;
            return -1;
        }
        total_recv += static_cast<size_t>(n);
    }

    return 0;
}

void* TcpConnection::get_ssl() const
{
    if (tls_conn_) {
        return tls_conn_->native_handle();
    }
    return nullptr;
}

void TcpConnection::set_auto_reconnect(bool enable, int max_retries)
{
    auto_reconnect_ = enable;
    max_retries_ = max_retries;
    LOG_INFO("自动重连 %s (最大重试: %s)",
             enable ? "已启用" : "已禁用",
             max_retries < 0 ? "无限" : std::to_string(max_retries).c_str());
}

bool TcpConnection::reconnect()
{
    if (!auto_reconnect_) return false;

    if (max_retries_ >= 0 && reconnect_count_ >= max_retries_) {
        LOG_ERROR("重连已达上限 (%d 次)", max_retries_);
        return false;
    }

    // 指数退避延迟
    int delay_ms = kInitialReconnectDelayMs;
    for (int i = 0; i < reconnect_count_; i++) {
        delay_ms *= kReconnectBackoffFactor;
        if (delay_ms >= kMaxReconnectDelayMs) {
            delay_ms = kMaxReconnectDelayMs;
            break;
        }
    }

    reconnect_count_++;
    LOG_INFO("重连中 (%d/%s)... 等待 %dms",
             reconnect_count_,
             (max_retries_ < 0) ? "无限" : std::to_string(max_retries_).c_str(),
             delay_ms);

#ifdef _WIN32
    Sleep(static_cast<DWORD>(delay_ms));
#else
    struct timespec ts;
    ts.tv_sec = delay_ms / 1000;
    ts.tv_nsec = (delay_ms % 1000) * 1000000;
    nanosleep(&ts, nullptr);
#endif

    bool result = connect(host_, port_, use_tls_);
    if (result) {
        LOG_INFO("重连成功 (%d 次尝试后)", reconnect_count_);
        reconnect_count_ = 0;
    }

    return result;
}

} // namespace network
} // namespace client
} // namespace chrono
