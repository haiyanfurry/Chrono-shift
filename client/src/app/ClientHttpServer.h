/**
 * Chrono-shift 客户端本地 HTTP 服务
 * C++17 重构版
 *
 * 轻量级 HTTP 服务器，监听 127.0.0.1
 * 为 WebView2 前端提供本地 API 桥接、健康检查、状态查询
 */
#ifndef CHRONO_CLIENT_CLIENT_HTTP_SERVER_H
#define CHRONO_CLIENT_CLIENT_HTTP_SERVER_H

#include <cstdint>
#include <string>
#include <atomic>
#include <thread>

#include <winsock2.h>
#include <windows.h>

namespace chrono {
namespace client {
namespace app {

/**
 * 客户端本地 HTTP 服务
 *
 * 监听 127.0.0.1 上的指定端口，提供:
 * 1. 健康检查 (/health)
 * 2. 本地状态 (/api/local/status)
 * 3. 可扩展的路由注册
 *
 * 注意: 仅监听 127.0.0.1，不对外暴露。
 * 使用独立线程运行，支持 start/stop 生命周期管理。
 */
class ClientHttpServer {
public:
    /** 默认端口 */
    static constexpr uint16_t kDefaultPort = 9010;

    /** 请求缓冲区大小 */
    static constexpr size_t kMaxBufSize = 8192;

    /** 监听队列长度 */
    static constexpr int kListenBacklog = 10;

    ClientHttpServer();
    ~ClientHttpServer();

    /* 禁止拷贝 */
    ClientHttpServer(const ClientHttpServer&) = delete;
    ClientHttpServer& operator=(const ClientHttpServer&) = delete;

    /* 允许移动 */
    ClientHttpServer(ClientHttpServer&& other) noexcept;
    ClientHttpServer& operator=(ClientHttpServer&& other) noexcept;

    // ============================================================
    // 生命周期
    // ============================================================

    /**
     * 启动服务
     * @param port 监听端口 (默认 9010)
     * @return true=启动成功
     */
    bool start(uint16_t port = kDefaultPort);

    /**
     * 停止服务
     */
    void stop();

    /**
     * 是否正在运行
     */
    bool is_running() const;

    /**
     * 获取监听端口
     */
    uint16_t get_port() const;

private:
    // ============================================================
    // HTTP 处理
    // ============================================================

    /** 服务线程入口 */
    void server_loop();

    /** 处理单个客户端连接 */
    void handle_client(SOCKET fd);

    /** 发送 HTTP 响应 */
    void send_response(SOCKET fd, int status_code,
                       const std::string& status_text,
                       const std::string& content_type,
                       const std::string& body);

    /** 发送 JSON 响应 */
    void send_json_response(SOCKET fd, int status_code,
                            const std::string& status_text,
                            const std::string& json_body);

    /** 发送错误 JSON */
    void send_error_json(SOCKET fd, int status_code,
                         const std::string& message);

    // ============================================================
    // 路由处理器
    // ============================================================

    /** GET /health */
    void handle_health(SOCKET fd);

    /** GET /api/local/status */
    void handle_local_status(SOCKET fd);

    /** 404 */
    void handle_not_found(SOCKET fd);

    // ============================================================
    // 成员
    // ============================================================

    /** 监听 socket */
    SOCKET listen_fd_ = INVALID_SOCKET;

    /** 监听端口 */
    uint16_t port_ = 0;

    /** 运行标志 */
    std::atomic<bool> running_{false};

    /** 服务线程 */
    std::thread server_thread_;
};

} // namespace app
} // namespace client
} // namespace chrono

#endif // CHRONO_CLIENT_CLIENT_HTTP_SERVER_H
