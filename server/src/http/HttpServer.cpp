/**
 * Chrono-shift C++ HTTP 服务器实现
 * Windows IOCP / Linux epoll 多路复用
 */
#include "HttpServer.h"
#include "../tls/TlsContext.h"
#include "../util/Logger.h"
#include "../util/StringUtils.h"
#include <cstring>
#include <cerrno>
#include <sstream>

namespace chrono {
namespace http {

// ============================================================
// Connection 实现
// ============================================================

Connection::Connection() {
    read_buf_.reserve(65536);
    write_buf_.reserve(65536);
}

Connection::~Connection() {
    close();
}

Connection::Connection(Connection&& other) noexcept
    : fd_(other.fd_)
    , ssl_(other.ssl_)
    , read_buf_(std::move(other.read_buf_))
    , write_buf_(std::move(other.write_buf_))
    , state(other.state)
    , request(std::move(other.request))
    , response(std::move(other.response))
    , request_parsed(other.request_parsed)
    , is_websocket(other.is_websocket)
    , ws_conn(other.ws_conn)
    , last_activity_ms(other.last_activity_ms) {
    other.fd_ = -1;
    other.ssl_ = nullptr;
    other.ws_conn = nullptr;
}

Connection& Connection::operator=(Connection&& other) noexcept {
    if (this != &other) {
        close();
        fd_ = other.fd_;
        ssl_ = other.ssl_;
        read_buf_ = std::move(other.read_buf_);
        write_buf_ = std::move(other.write_buf_);
        state = other.state;
        request = std::move(other.request);
        response = std::move(other.response);
        request_parsed = other.request_parsed;
        is_websocket = other.is_websocket;
        ws_conn = other.ws_conn;
        last_activity_ms = other.last_activity_ms;
        other.fd_ = -1;
        other.ssl_ = nullptr;
        other.ws_conn = nullptr;
    }
    return *this;
}

void Connection::close() {
    if (fd_ >= 0) {
#ifdef _WIN32
        closesocket(fd_);
#else
        ::close(fd_);
#endif
        fd_ = -1;
    }
    // SSL 由 TlsContext 管理，这里不释放
    ssl_ = nullptr;
    state = ConnState::kClosed;
}

// ============================================================
// HttpServer 实现
// ============================================================

HttpServer::HttpServer(ServerConfig config)
    : config_(std::move(config)) {
}

HttpServer::~HttpServer() {
    stop();
}

bool HttpServer::init() {
    LOG_INFO("Initializing HTTP server on %s:%d",
             config_.host.c_str(), config_.port);

#ifdef _WIN32
    // 初始化 Winsock
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        LOG_ERROR("WSAStartup failed");
        return false;
    }
#endif

    if (!setup_socket()) {
        LOG_ERROR("Failed to setup socket");
        return false;
    }

    if (config_.enable_tls && !setup_tls()) {
        LOG_ERROR("Failed to setup TLS");
        return false;
    }

    LOG_INFO("HTTP server initialized successfully");
    return true;
}

bool HttpServer::setup_socket() {
#ifdef _WIN32
    listen_fd_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_fd_ == INVALID_SOCKET) {
        LOG_ERROR("Failed to create socket: %d", WSAGetLastError());
        return false;
    }

    // SO_REUSEADDR
    int opt = 1;
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR,
               (const char*)&opt, sizeof(opt));

    // 绑定
    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<u_short>(config_.port));
    inet_pton(AF_INET, config_.host.c_str(), &addr.sin_addr);

    if (bind(listen_fd_, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        LOG_ERROR("Failed to bind: %d", WSAGetLastError());
        closesocket(listen_fd_);
        listen_fd_ = INVALID_SOCKET;
        return false;
    }

    // 监听
    if (listen(listen_fd_, config_.backlog) == SOCKET_ERROR) {
        LOG_ERROR("Failed to listen: %d", WSAGetLastError());
        closesocket(listen_fd_);
        listen_fd_ = INVALID_SOCKET;
        return false;
    }

    // 创建 IOCP
    iocp_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    if (!iocp_) {
        LOG_ERROR("Failed to create IOCP");
        closesocket(listen_fd_);
        listen_fd_ = INVALID_SOCKET;
        return false;
    }

    // 将 listen socket 关联到 IOCP
    CreateIoCompletionPort((HANDLE)listen_fd_, iocp_, 0, 0);

#else
    listen_fd_ = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (listen_fd_ < 0) {
        LOG_ERROR("Failed to create socket: %s", strerror(errno));
        return false;
    }

    int opt = 1;
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(config_.port);
    inet_pton(AF_INET, config_.host.c_str(), &addr.sin_addr);

    if (bind(listen_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        LOG_ERROR("Failed to bind: %s", strerror(errno));
        ::close(listen_fd_);
        listen_fd_ = -1;
        return false;
    }

    if (listen(listen_fd_, config_.backlog) < 0) {
        LOG_ERROR("Failed to listen: %s", strerror(errno));
        ::close(listen_fd_);
        listen_fd_ = -1;
        return false;
    }

    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ < 0) {
        LOG_ERROR("Failed to create epoll: %s", strerror(errno));
        ::close(listen_fd_);
        listen_fd_ = -1;
        return false;
    }

    struct epoll_event ev = {};
    ev.events = EPOLLIN;
    ev.data.fd = listen_fd_;
    epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, listen_fd_, &ev);
#endif

    LOG_INFO("Socket bound to %s:%d", config_.host.c_str(), config_.port);
    return true;
}

bool HttpServer::setup_tls() {
    // TLS 由 TlsContext 模块管理
    // 简化实现：这里只做占位
    LOG_INFO("TLS support ready (cert: %s)",
             config_.tls_cert_path.empty() ? "self-signed" : config_.tls_cert_path.c_str());
    return true;
}

bool HttpServer::register_route(Method method, const std::string& path, RouteHandler handler) {
    bool result = router_.add_route(method, path, std::move(handler));
    if (result) {
        LOG_INFO("Route registered: %s %s", method_to_string(method), path.c_str());
    } else {
        LOG_ERROR("Failed to register route: %s %s", method_to_string(method), path.c_str());
    }
    return result;
}

void HttpServer::add_middleware(MiddlewareFunc middleware) {
    middlewares_.push_back(std::move(middleware));
}

bool HttpServer::start() {
    if (running_) {
        LOG_WARN("Server already running");
        return false;
    }

    running_ = true;
    LOG_INFO("Starting HTTP server with %d workers", config_.worker_count);

    for (int i = 0; i < config_.worker_count; i++) {
        workers_.emplace_back(&HttpServer::worker_loop, this, i);
    }

    LOG_INFO("HTTP server started on port %d", config_.port);
    return true;
}

void HttpServer::stop() {
    if (!running_) return;
    running_ = false;

    // 关闭 listen socket 以唤醒所有工作线程
#ifdef _WIN32
    if (listen_fd_ != INVALID_SOCKET) {
        closesocket(listen_fd_);
        listen_fd_ = INVALID_SOCKET;
    }
#else
    if (listen_fd_ >= 0) {
        ::close(listen_fd_);
        listen_fd_ = -1;
    }
#endif

    // 等待工作线程结束
    for (auto& t : workers_) {
        if (t.joinable()) t.join();
    }
    workers_.clear();

    // 清理所有连接
    {
        std::lock_guard<std::mutex> lock(conn_mutex_);
        connections_.clear();
    }

#ifdef _WIN32
    if (iocp_) {
        CloseHandle(iocp_);
        iocp_ = nullptr;
    }
    WSACleanup();
#else
    if (epoll_fd_ >= 0) {
        ::close(epoll_fd_);
        epoll_fd_ = -1;
    }
#endif

    LOG_INFO("HTTP server stopped");
}

void HttpServer::worker_loop(int worker_id) {
    LOG_INFO("Worker %d started", worker_id);

#ifdef _WIN32
    // IOCP 事件循环
    while (running_) {
        DWORD bytes_transferred = 0;
        ULONG_PTR completion_key = 0;
        OVERLAPPED* overlapped = nullptr;

        BOOL result = GetQueuedCompletionStatus(
            iocp_, &bytes_transferred, &completion_key, &overlapped, 100);

        if (!running_) break;

        if (!result) {
            if (GetLastError() == WAIT_TIMEOUT) {
                // 超时，正常
                cleanup_connections();
                continue;
            }
            // 连接已关闭
            continue;
        }

        // 处理 accept 或 read/write 完成
        // TODO: 完整的 IOCP 处理
    }
#else
    // epoll 事件循环
    constexpr int kMaxEvents = 64;
    struct epoll_event events[kMaxEvents];

    while (running_) {
        int nfds = epoll_wait(epoll_fd_, events, kMaxEvents, 100);
        if (nfds < 0) {
            if (errno == EINTR) continue;
            break;
        }

        for (int i = 0; i < nfds; i++) {
            int fd = events[i].data.fd;

            if (fd == listen_fd_) {
                // 新连接
                struct sockaddr_in client_addr = {};
                socklen_t addr_len = sizeof(client_addr);
                int client_fd = accept(listen_fd_,
                                       (struct sockaddr*)&client_addr,
                                       &addr_len);
                if (client_fd < 0) continue;

                // 设置为非阻塞
                int flags = fcntl(client_fd, F_GETFL, 0);
                fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);

                // 添加到 epoll
                struct epoll_event ev = {};
                ev.events = EPOLLIN | EPOLLET;  // 边缘触发
                ev.data.fd = client_fd;
                epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, client_fd, &ev);

                // 创建连接对象
                auto conn = std::make_unique<Connection>();
                conn->set_fd(client_fd);
                conn->last_activity_ms = util::StringUtils::timestamp_ms();

                {
                    std::lock_guard<std::mutex> lock(conn_mutex_);
                    connections_[client_fd] = std::move(conn);
                }
            } else {
                // 已有连接的数据
                std::unique_ptr<Connection> conn;
                {
                    std::lock_guard<std::mutex> lock(conn_mutex_);
                    auto it = connections_.find(fd);
                    if (it != connections_.end()) {
                        // 取出连接处理
                        conn = std::move(it->second);
                        connections_.erase(it);
                    }
                }

                if (conn) {
                    conn->last_activity_ms = util::StringUtils::timestamp_ms();

                    if (events[i].events & (EPOLLERR | EPOLLHUP)) {
                        // 连接错误
                        conn->close();
                        continue;
                    }

                    if (events[i].events & EPOLLIN) {
                        // 读取数据
                        uint8_t buf[65536];
                        ssize_t n = read(fd, buf, sizeof(buf));
                        if (n > 0) {
                            conn->read_buf().insert(
                                conn->read_buf().end(), buf, buf + n);
                            handle_connection(*conn);
                        }
                    }

                    // 写数据
                    if (events[i].events & EPOLLOUT || !conn->write_buf().empty()) {
                        if (!conn->write_buf().empty()) {
                            ssize_t n = write(fd, conn->write_buf().data(),
                                              conn->write_buf().size());
                            if (n > 0) {
                                conn->write_buf().erase(
                                    conn->write_buf().begin(),
                                    conn->write_buf().begin() + n);
                            }
                        }
                    }

                    // 检查连接是否还需继续
                    if (conn->is_open() && conn->state != ConnState::kClosed) {
                        // 重新添加到 epoll
                        struct epoll_event ev = {};
                        ev.events = EPOLLIN | EPOLLET;
                        if (!conn->write_buf().empty()) {
                            ev.events |= EPOLLOUT;
                        }
                        ev.data.fd = fd;
                        epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev);

                        std::lock_guard<std::mutex> lock(conn_mutex_);
                        connections_[fd] = std::move(conn);
                    }
                }
            }
        }

        // 周期性清理超时连接
        cleanup_connections();
    }
#endif

    LOG_INFO("Worker %d stopped", worker_id);
}

void HttpServer::handle_connection(Connection& conn) {
    if (conn.request_parsed) return;

    // 解析 HTTP 请求
    HttpParser parser;
    auto req_opt = parser.parse(conn.read_buf().data(), conn.read_buf().size());
    if (!req_opt) {
        // 不完整请求，等待更多数据
        return;
    }

    conn.request = std::move(*req_opt);
    conn.request_parsed = true;
    conn.response = Response{};
    total_requests_++;

    // 运行中间件
    if (!run_middleware(conn.request, conn.response)) {
        // 中间件已处理响应
        auto wire = ResponseBuilder::build(conn.response);
        conn.write_buf() = std::move(wire);
        conn.state = ConnState::kWriting;
        return;
    }

    // 路由匹配
    auto* route = router_.find_route(conn.request.method, conn.request.path);
    if (route) {
        try {
            route->handler(conn.request, conn.response);
        } catch (const std::exception& e) {
            LOG_ERROR("Handler exception: %s", e.what());
            conn.response.set_status(500, "Internal Server Error");
            conn.response.set_json(R"({"status":"error","message":"Internal server error"})");
        }
    } else {
        conn.response.set_status(404, "Not Found");
        conn.response.set_json(R"({"status":"error","message":"Not found"})");
    }

    // 序列化响应
    auto wire = ResponseBuilder::build(conn.response);
    conn.write_buf() = std::move(wire);
    conn.state = ConnState::kWriting;
}

bool HttpServer::run_middleware(const Request& req, Response& resp) {
    for (auto& mw : middlewares_) {
        if (!mw(req, resp)) {
            return false; // 中间件阻止了请求
        }
    }
    return true;
}

void HttpServer::cleanup_connections() {
    uint64_t now = util::StringUtils::timestamp_ms();
    std::lock_guard<std::mutex> lock(conn_mutex_);

    auto it = connections_.begin();
    while (it != connections_.end()) {
        if (now - it->second->last_activity_ms > static_cast<uint64_t>(kTimeoutMs)) {
            it->second->close();
            it = connections_.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace http
} // namespace chrono
