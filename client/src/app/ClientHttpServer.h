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
#include <functional>
#include <unordered_map>
#include <atomic>
#include <thread>
#include <memory>       /* std::unique_ptr */

#include <winsock2.h>
#include <windows.h>

/* OpenSSL 前向声明 (避免在头文件中引入 OpenSSL 头) */
struct ssl_st;

namespace chrono {
namespace client {
namespace app {

/* TLS 服务端上下文前向声明 */
class TlsServerContext;

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

    /** 路由处理器类型 (fd, path, method, body) -> void */
    using HttpHandler = std::function<void(SOCKET fd, const std::string& path,
                                           const std::string& method,
                                           const std::string& body)>;

    // === 预留路由前缀 (扩展/插件/AI 使用) ===
    static constexpr const char* kPluginRoutePrefix   = "/api/plugins/";    // 插件路由
    static constexpr const char* kExtensionRoutePrefix = "/api/ext/";       // 扩展路由
    static constexpr const char* kAIRoutePrefix        = "/api/ai/";        // AI 路由
    static constexpr const char* kDevToolRoutePrefix   = "/api/devtools/";  // 开发者工具路由

    ClientHttpServer();
    ~ClientHttpServer();

    /* 禁止拷贝 */
    ClientHttpServer(const ClientHttpServer&) = delete;
    ClientHttpServer& operator=(const ClientHttpServer&) = delete;

    /* 允许移动 */
    ClientHttpServer(ClientHttpServer&& other) noexcept;
    ClientHttpServer& operator=(ClientHttpServer&& other) noexcept;

    // ============================================================
    // TLS / HTTPS 支持
    // ============================================================

    /**
     * 设置 TLS 证书路径 (需在 start() 之前调用)
     * @param cert_file  PEM 证书文件路径
     * @param key_file   PEM 私钥文件路径
     */
    void set_tls_cert_paths(const std::string& cert_file, const std::string& key_file);

    /**
     * 启用或禁用 HTTPS 模式
     * @param enable true=启用 HTTPS, false=使用普通 HTTP
     */
    void set_use_https(bool enable);

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

    // ============================================================
    // 动态路由注册 (扩展/插件/AI 接口)
    // ============================================================

    /**
     * 注册自定义路由处理器
     * @param path_prefix  路由前缀 (如 "/api/plugins/")
     * @param handler      处理回调 (fd, path, method, body)
     * @return 0=成功, -1=已达最大路由数
     */
    int register_route(const std::string& path_prefix, HttpHandler handler);

    /**
     * 注销路由处理器
     * @param path_prefix 路由前缀
     */
    void unregister_route(const std::string& path_prefix);

    /**
     * 检查路径是否匹配预留路由前缀
     * @param path HTTP 请求路径
     * @return true 如果是预留路由
     */
    bool is_reserved_route(const std::string& path) const;

    /** 最大动态路由数 */
    static constexpr size_t kMaxDynamicRoutes = 64;

private:
    // ============================================================
    // HTTP 处理
    // ============================================================

    /** 服务线程入口 */
    void server_loop();

    /** 处理单个普通 TCP 客户端连接 */
    void handle_client(SOCKET fd);

    /** 处理单个 TLS 客户端连接 */
    void handle_client_tls(SOCKET fd, struct ssl_st* ssl);

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

    /** 内部: 向 fd 或 ssl 发送裸数据 */
    void send_raw(const void* data, size_t len);

    // ============================================================
    // 路由处理器
    // ============================================================

    /** GET /health */
    void handle_health(SOCKET fd);

    /** GET /api/local/status */
    void handle_local_status(SOCKET fd);

    /** 分发到动态路由 */
    bool dispatch_dynamic_route(SOCKET fd, const std::string& path,
                                const std::string& method,
                                const std::string& body);

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

    /** 动态路由表 (path_prefix -> handler) */
    std::unordered_map<std::string, HttpHandler> dynamic_routes_;

    // ============================================================
    // TLS / HTTPS 成员
    // ============================================================

    /** TLS 服务端上下文 (仅 HTTPS 模式使用) */
    std::unique_ptr<TlsServerContext> tls_ctx_;

    /** 是否启用 HTTPS */
    bool use_https_ = false;

    /** 当前 TLS 连接 (handle_client_tls 生命周期内非空) */
    struct ssl_st* current_ssl_ = nullptr;

    /** TLS 证书路径 */
    std::string tls_cert_file_;
    std::string tls_key_file_;
};

} // namespace app
} // namespace client
} // namespace chrono

#endif // CHRONO_CLIENT_CLIENT_HTTP_SERVER_H
