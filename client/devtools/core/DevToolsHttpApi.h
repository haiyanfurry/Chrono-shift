/**
 * DevToolsHttpApi.h — 开发者模式 HTTP API 路由处理器
 *
 * 通过 ClientHttpServer 的 /api/devtools/ 路由前缀提供以下端点：
 *
 * | 端点                               | 方法  | 功能                 |
 * |-----------------------------------|-------|---------------------|
 * | POST /api/devtools/exec           | POST  | 执行 CLI 命令        |
 * | GET  /api/devtools/commands       | GET   | 列出所有可用命令      |
 * | GET  /api/devtools/status         | GET   | 获取开发者模式状态    |
 * | POST /api/devtools/enable         | POST  | 启用开发者模式        |
 * | POST /api/devtools/disable        | POST  | 禁用开发者模式        |
 * | GET  /api/devtools/network/status | GET   | 网络连接状态          |
 * | GET  /api/devtools/storage/list   | GET   | 存储内容列表          |
 * | POST /api/devtools/endpoint/test  | POST  | 测试 API 端点         |
 * | GET  /api/devtools/ws/status      | GET   | WebSocket 状态        |
 * | POST /api/devtools/ws/send        | POST  | 发送 WebSocket 消息   |
 *
 * 所有响应格式：
 *   { "status": "ok"|"error", "data": {...}, "timestamp": 1234567890 }
 */
#ifndef CHRONO_CLIENT_DEVTOOLS_HTTP_API_H
#define CHRONO_CLIENT_DEVTOOLS_HTTP_API_H

#include <string>
#include <winsock2.h>

namespace chrono {
namespace client {
namespace app {
    class ClientHttpServer;
} // namespace app

namespace devtools {

class DevToolsEngine;

/**
 * HTTP API 路由处理器
 *
 * 注册到 ClientHttpServer 的 /api/devtools/ 路由前缀，
 * 解析子路径并调用 DevToolsEngine 执行命令或查询状态。
 */
class DevToolsHttpApi {
public:
    explicit DevToolsHttpApi(DevToolsEngine& engine);
    ~DevToolsHttpApi() = default;

    /* 禁止拷贝 */
    DevToolsHttpApi(const DevToolsHttpApi&) = delete;
    DevToolsHttpApi& operator=(const DevToolsHttpApi&) = delete;

    /**
     * 注册路由到 HTTP 服务器
     * @param http_server 客户端 HTTP 服务器引用
     */
    void register_routes(app::ClientHttpServer& http_server);

    /**
     * 从 HTTP 服务器注销路由
     */
    void unregister_routes(app::ClientHttpServer& http_server);

private:
    /** 处理 devtools 请求的入口回调 */
    void handle_request(SOCKET fd, const std::string& path,
                        const std::string& method, const std::string& body);

    /* === 端点处理器 === */

    void handle_exec(SOCKET fd, const std::string& body);
    void handle_commands(SOCKET fd);
    void handle_status(SOCKET fd);
    void handle_enable(SOCKET fd);
    void handle_disable(SOCKET fd);
    void handle_network_status(SOCKET fd);
    void handle_storage_list(SOCKET fd);
    void handle_endpoint_test(SOCKET fd, const std::string& body);
    void handle_ws_status(SOCKET fd);
    void handle_ws_send(SOCKET fd, const std::string& body);

    /** 转义字符串中的特殊字符为 JSON 安全格式 */
    static std::string escape_json(const std::string& raw);

    /** 构造原始 HTTP JSON 响应并发送 (因 ClientHttpServer::send_json_response 为 private) */
    void send_raw_json(SOCKET fd, int code, const std::string& status,
                       const std::string& json_body);

    /** 构造原始 HTTP 错误响应并发送 */
    void send_raw_error(SOCKET fd, int code, const std::string& message);

    DevToolsEngine& engine_;
};

} // namespace devtools
} // namespace client
} // namespace chrono

#endif // CHRONO_CLIENT_DEVTOOLS_HTTP_API_H
