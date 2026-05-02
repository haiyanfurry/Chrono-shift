/**
 * Chrono-shift 客户端本地 HTTP 服务实现
 * C++17 重构版
 *
 * 轻量级 HTTP 服务器，监听 127.0.0.1
 * 为 WebView2 前端提供本地 API 桥接
 */

#include "ClientHttpServer.h"

#include <cstring>
#include <sstream>

#include "../util/Logger.h"

namespace chrono {
namespace client {
namespace app {

/* ============================================================
 * 构造函数 / 析构函数
 * ============================================================ */

ClientHttpServer::ClientHttpServer()
    : listen_fd_(INVALID_SOCKET)
    , port_(0)
{
}

ClientHttpServer::~ClientHttpServer()
{
    stop();
}

/* ============================================================
 * 移动语义
 * ============================================================ */

ClientHttpServer::ClientHttpServer(ClientHttpServer&& other) noexcept
    : listen_fd_(other.listen_fd_)
    , port_(other.port_)
    , running_(other.running_.load())
{
    other.listen_fd_ = INVALID_SOCKET;
    other.port_ = 0;
    other.running_ = false;
}

ClientHttpServer& ClientHttpServer::operator=(ClientHttpServer&& other) noexcept
{
    if (this != &other) {
        stop();
        listen_fd_ = other.listen_fd_;
        port_ = other.port_;
        running_ = other.running_.load();
        other.listen_fd_ = INVALID_SOCKET;
        other.port_ = 0;
        other.running_ = false;
    }
    return *this;
}

/* ============================================================
 * 生命周期
 * ============================================================ */

bool ClientHttpServer::start(uint16_t port)
{
    if (running_) {
        LOG_WARN("本地 HTTP 服务已在运行中 (端口 %u)", port_);
        return true;
    }

    /* 创建 socket */
    listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ == INVALID_SOCKET) {
        LOG_ERROR("创建本地 HTTP 服务 socket 失败: %d", WSAGetLastError());
        return false;
    }

    /* 允许端口重用 */
    int optval = 1;
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&optval), sizeof(optval));

    /* 绑定 127.0.0.1:port */
    struct sockaddr_in addr = {};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port        = htons(port);

    if (bind(listen_fd_, reinterpret_cast<struct sockaddr*>(&addr),
             sizeof(addr)) != 0) {
        LOG_ERROR("绑定本地 HTTP 服务端口 %u 失败: %d", port, WSAGetLastError());
        closesocket(listen_fd_);
        listen_fd_ = INVALID_SOCKET;
        return false;
    }

    /* 监听 */
    if (listen(listen_fd_, kListenBacklog) != 0) {
        LOG_ERROR("本地 HTTP 服务 listen 失败: %d", WSAGetLastError());
        closesocket(listen_fd_);
        listen_fd_ = INVALID_SOCKET;
        return false;
    }

    port_ = port;
    running_ = true;

    /* 启动服务线程 */
    try {
        server_thread_ = std::thread(&ClientHttpServer::server_loop, this);
    } catch (const std::exception& e) {
        LOG_ERROR("创建本地 HTTP 服务线程失败: %s", e.what());
        running_ = false;
        closesocket(listen_fd_);
        listen_fd_ = INVALID_SOCKET;
        return false;
    }

    LOG_INFO("客户端本地 HTTP 服务已启动: 127.0.0.1:%u", port);
    return true;
}

void ClientHttpServer::stop()
{
    if (!running_) return;

    running_ = false;

    /* 关闭监听 socket 以中断 accept */
    if (listen_fd_ != INVALID_SOCKET) {
        closesocket(listen_fd_);
        listen_fd_ = INVALID_SOCKET;
    }

    /* 等待线程结束 */
    if (server_thread_.joinable()) {
        server_thread_.join();
    }

    LOG_INFO("客户端本地 HTTP 服务已停止");
}

bool ClientHttpServer::is_running() const
{
    return running_;
}

uint16_t ClientHttpServer::get_port() const
{
    return port_;
}

/* ============================================================
 * 服务线程
 * ============================================================ */

void ClientHttpServer::server_loop()
{
    while (running_) {
        struct sockaddr_in client_addr = {};
        int addr_len = sizeof(client_addr);

        SOCKET client_fd = accept(
            listen_fd_,
            reinterpret_cast<struct sockaddr*>(&client_addr),
            &addr_len);

        if (client_fd == INVALID_SOCKET) {
            if (running_) {
                LOG_DEBUG("本地 HTTP 服务 accept 失败: %d", WSAGetLastError());
            }
            break;
        }

        handle_client(client_fd);
    }
}

/* ============================================================
 * 客户端处理
 * ============================================================ */

void ClientHttpServer::handle_client(SOCKET fd)
{
    char buf[kMaxBufSize] = {};
    int received = recv(fd, buf, sizeof(buf) - 1, 0);
    if (received <= 0) {
        closesocket(fd);
        return;
    }
    buf[received] = '\0';

    /* 解析请求行: METHOD /path HTTP/1.1 */
    char method[16] = {}, path[256] = {};
    if (sscanf(buf, "%15s %255s", method, path) < 2) {
        send_error_json(fd, 400, "Bad Request");
        closesocket(fd);
        return;
    }

    /* 提取 body (简单解析: 空行后为 body) */
    std::string request_body;
    const char* body_start = std::strstr(buf, "\r\n\r\n");
    if (body_start) {
        body_start += 4;
        request_body = std::string(body_start, received - (body_start - buf));
    }

    /* 优先尝试动态路由分发 (扩展/插件/AI 路由) */
    if (dispatch_dynamic_route(fd, path, method, request_body)) {
        closesocket(fd);
        return;
    }

    /* 静态路由匹配 */
    if (std::strcmp(method, "GET") == 0) {
        if (std::strcmp(path, "/health") == 0) {
            handle_health(fd);
        } else if (std::strcmp(path, "/api/local/status") == 0) {
            handle_local_status(fd);
        } else {
            handle_not_found(fd);
        }
    } else {
        send_error_json(fd, 405, "Method Not Allowed");
    }

    closesocket(fd);
}

/* ============================================================
 * HTTP 响应辅助
 * ============================================================ */

void ClientHttpServer::send_response(SOCKET fd, int status_code,
                                     const std::string& status_text,
                                     const std::string& content_type,
                                     const std::string& body)
{
    std::ostringstream header;
    header << "HTTP/1.1 " << status_code << " " << status_text << "\r\n"
           << "Content-Type: " << content_type << "\r\n"
           << "Content-Length: " << body.size() << "\r\n"
           << "Connection: close\r\n"
           << "Access-Control-Allow-Origin: *\r\n"
           << "\r\n";

    std::string header_str = header.str();
    ::send(fd, header_str.data(), static_cast<int>(header_str.size()), 0);
    if (!body.empty()) {
        ::send(fd, body.data(), static_cast<int>(body.size()), 0);
    }
}

/* ============================================================
 * JSON 字符串转义工具
 * ============================================================ */

static std::string escapeJson(const std::string& s)
{
    std::ostringstream out;
    out << '"';
    for (char c : s) {
        switch (c) {
            case '"':  out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\b': out << "\\b";  break;
            case '\f': out << "\\f";  break;
            case '\n': out << "\\n";  break;
            case '\r': out << "\\r";  break;
            case '\t': out << "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out << buf;
                } else {
                    out << c;
                }
                break;
        }
    }
    out << '"';
    return out.str();
}

void ClientHttpServer::send_json_response(SOCKET fd, int status_code,
                                          const std::string& status_text,
                                          const std::string& json_body)
{
    send_response(fd, status_code, status_text, "application/json", json_body);
}

void ClientHttpServer::send_error_json(SOCKET fd, int status_code,
                                       const std::string& message)
{
    std::string body = "{\"status\":\"error\",\"message\":"
                     + escapeJson(message) + "}";
    send_json_response(fd, status_code, "Error", body);
}

/* ============================================================
 * 路由处理器
 * ============================================================ */

void ClientHttpServer::handle_health(SOCKET fd)
{
    const char* json = "{\"status\":\"ok\",\"service\":\"chrono-client-local\"}";
    send_json_response(fd, 200, "OK", json);
}

void ClientHttpServer::handle_local_status(SOCKET fd)
{
    const char* json = "{"
        "\"status\":\"running\","
        "\"version\":\"0.1.0\""
        "}";
    send_json_response(fd, 200, "OK", json);
}

void ClientHttpServer::handle_not_found(SOCKET fd)
{
    send_error_json(fd, 404, "Not Found");
}

/* ============================================================
 * 动态路由 (扩展/插件/AI)
 * ============================================================ */

int ClientHttpServer::register_route(const std::string& path_prefix, HttpHandler handler)
{
    if (dynamic_routes_.size() >= kMaxDynamicRoutes) {
        LOG_WARN("动态路由表已满 (%zu/%zu)", dynamic_routes_.size(), kMaxDynamicRoutes);
        return -1;
    }
    if (dynamic_routes_.find(path_prefix) != dynamic_routes_.end()) {
        LOG_WARN("路由前缀已存在: %s", path_prefix.c_str());
        return -1;
    }
    dynamic_routes_[path_prefix] = std::move(handler);
    LOG_INFO("注册动态路由: %s (总计 %zu)", path_prefix.c_str(), dynamic_routes_.size());
    return 0;
}

void ClientHttpServer::unregister_route(const std::string& path_prefix)
{
    auto it = dynamic_routes_.find(path_prefix);
    if (it != dynamic_routes_.end()) {
        dynamic_routes_.erase(it);
        LOG_INFO("注销动态路由: %s", path_prefix.c_str());
    }
}

bool ClientHttpServer::is_reserved_route(const std::string& path) const
{
    return path.find(kPluginRoutePrefix)   == 0 ||
           path.find(kExtensionRoutePrefix) == 0 ||
           path.find(kAIRoutePrefix)        == 0 ||
           path.find(kDevToolRoutePrefix)   == 0;
}

bool ClientHttpServer::dispatch_dynamic_route(SOCKET fd, const std::string& path,
                                              const std::string& method,
                                              const std::string& body)
{
    for (const auto& [prefix, handler] : dynamic_routes_) {
        if (path.find(prefix) == 0) {
            handler(fd, path, method, body);
            return true;
        }
    }
    return false;
}

} // namespace app
} // namespace client
} // namespace chrono
