/**
 * Chrono-shift C++ HTTP 服务器
 * RAII 管理 socket 和 SSL 上下文
 * 支持 IOCP (Windows) / epoll (Linux)
 * C++17 重构版
 */
#ifndef CHRONO_CPP_HTTP_SERVER_H
#define CHRONO_CPP_HTTP_SERVER_H

#include "HttpTypes.h"
#include "HttpParser.h"
#include "../util/Logger.h"
#include <string>
#include <atomic>
#include <thread>
#include <vector>
#include <memory>
#include <functional>
#include <mutex>
#include <map>

// 平台兼容
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#endif

#include "../tls/TlsContext.h"

namespace chrono {
namespace http {

using chrono::tls::TlsContext;

/**
 * 服务器配置
 */
struct ServerConfig {
    std::string host = "0.0.0.0";
    int port = 4443;
    std::string tls_cert_path;
    std::string tls_key_path;
    std::string storage_path = "./server/data";
    std::string db_path = "./server/data";
    std::string jwt_secret = "chrono-shift-secret";
    int worker_count = 4;
    int backlog = 1024;
    bool enable_tls = true;
};

/**
 * 连接状态
 */
enum class ConnState {
    kReading,
    kWriting,
    kWsOpen,
    kClosed
};

/**
 * 连接对象 — RAII 管理
 */
class Connection {
public:
    Connection();
    ~Connection();

    // 禁止拷贝
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;

    // 允许移动
    Connection(Connection&& other) noexcept;
    Connection& operator=(Connection&& other) noexcept;

    void close();
    bool is_open() const { return fd_ != -1; }

    // 设置/获取 fd
    void set_fd(int fd) { fd_ = fd; }
    int fd() const { return fd_; }

    // SSL 指针 (不拥有)
    void set_ssl(void* ssl) { ssl_ = ssl; }
    void* ssl() const { return ssl_; }

    // 缓冲区
    std::vector<uint8_t>& read_buf() { return read_buf_; }
    std::vector<uint8_t>& write_buf() { return write_buf_; }

    // 状态
    ConnState state = ConnState::kReading;
    Request request;
    Response response;
    bool request_parsed = false;
    bool is_websocket = false;
    void* ws_conn = nullptr;
    uint64_t last_activity_ms = 0;

private:
    int fd_ = -1;
    void* ssl_ = nullptr;
    std::vector<uint8_t> read_buf_;
    std::vector<uint8_t> write_buf_;
};

#ifdef _WIN32
using SocketHandle = SOCKET;
constexpr SocketHandle kInvalidSocket = INVALID_SOCKET;
#else
using SocketHandle = int;
constexpr SocketHandle kInvalidSocket = -1;
#endif

/**
 * HTTP 服务器主类
 */
class HttpServer {
public:
    explicit HttpServer(ServerConfig config = ServerConfig{});
    ~HttpServer();

    // 禁止拷贝
    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;

    // 初始化
    bool init();

    // 路由注册
    bool register_route(Method method, const std::string& path, RouteHandler handler);

    // 启动/停止
    bool start();
    void stop();

    // 状态查询
    bool is_running() const { return running_; }
    uint64_t total_requests() const { return total_requests_; }

    // 获取配置
    const ServerConfig& config() const { return config_; }

    // 中间件支持
    using MiddlewareFunc = std::function<bool(const Request&, Response&)>;
    void add_middleware(MiddlewareFunc middleware);

private:
    // 服务器状态
    ServerConfig config_;
    Router router_;
    std::atomic<bool> running_{false};
    std::atomic<uint64_t> total_requests_{0};
    std::vector<std::thread> workers_;
    std::vector<MiddlewareFunc> middlewares_;

    // 网络资源
    SocketHandle listen_fd_ = kInvalidSocket;
#ifdef _WIN32
    HANDLE iocp_ = nullptr;
#else
    int epoll_fd_ = -1;
#endif

    // TLS
    std::unique_ptr<TlsContext> tls_;

    // 连接管理
    std::mutex conn_mutex_;
    std::map<int, std::unique_ptr<Connection>> connections_;

    // 内部方法
    bool setup_socket();
    bool setup_tls();
    void worker_loop(int worker_id);
    void handle_connection(Connection& conn);
    bool run_middleware(const Request& req, Response& resp);
    void cleanup_connections();
};

} // namespace http
} // namespace chrono

#endif // CHRONO_CPP_HTTP_SERVER_H
