/**
 * Chrono-shift 客户端 HTTP 连接封装
 * C++17 重构版
 */
#ifndef CHRONO_CLIENT_HTTP_CONNECTION_H
#define CHRONO_CLIENT_HTTP_CONNECTION_H

#include <cstdint>
#include <string>
#include <unordered_map>

namespace chrono {
namespace client {
namespace network {

class TcpConnection;

/**
 * HTTP/1.1 请求/响应封装
 * 基于 TcpConnection 发送 HTTP 请求并解析响应
 */
class HttpConnection {
public:
    /** HTTP 响应结构体 */
    struct Response {
        int status_code = 0;
        std::string status_text;
        std::string body;
        std::unordered_map<std::string, std::string> headers;
    };

    /**
     * @param tcp TCP 连接引用 (需外部管理生命周期)
     */
    explicit HttpConnection(TcpConnection& tcp);

    /**
     * 发送 HTTP 请求
     * @param method HTTP 方法 (GET/POST/PUT/DELETE 等)
     * @param path 请求路径
     * @param headers 额外请求头 (每行一个，不含 Host 和 Content-Length)
     * @param body 请求体 (可为 nullptr)
     * @param body_len 请求体长度
     * @return Response 结构体 (status_code=0 表示失败)
     */
    Response request(const std::string& method,
                     const std::string& path,
                     const std::string& headers = "",
                     const uint8_t* body = nullptr,
                     size_t body_len = 0);

private:
    TcpConnection& tcp_;
};

} // namespace network
} // namespace client
} // namespace chrono

#endif // CHRONO_CLIENT_HTTP_CONNECTION_H
