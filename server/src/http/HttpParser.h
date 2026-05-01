/**
 * Chrono-shift C++ HTTP 解析器
 * HTTP/1.1 请求解析 + 路由匹配
 * C++17 重构版
 */
#ifndef CHRONO_CPP_HTTP_PARSER_H
#define CHRONO_CPP_HTTP_PARSER_H

#include "HttpTypes.h"
#include <vector>
#include <string>
#include <optional>
#include <functional>
#include <memory>

namespace chrono {
namespace http {

/**
 * HTTP 解析器
 * 解析原始字节流到 HttpRequest
 */
class HttpParser {
public:
    HttpParser() = default;

    /**
     * 解析 HTTP 请求
     * @param data 原始字节数据
     * @param length 数据长度
     * @return 解析成功的 Request，失败返回 std::nullopt
     */
    std::optional<Request> parse(const uint8_t* data, size_t length);
    std::optional<Request> parse(const std::string& data);

    /**
     * 获取最后一个错误信息
     */
    const std::string& last_error() const { return error_; }

private:
    std::string error_;

    bool parse_request_line(Request& req, const std::string& line);
    bool parse_header_line(Request& req, const std::string& line);
};

/**
 * 路由条目
 */
struct RouteEntry {
    Method method;
    std::string path;
    RouteHandler handler;
};

/**
 * 路由表
 * 管理路由注册和匹配
 */
class Router {
public:
    Router() = default;

    /**
     * 注册路由
     */
    bool add_route(Method method, const std::string& path, RouteHandler handler);

    /**
     * 匹配路由
     * @return 匹配到的 RouteEntry，未匹配返回 nullptr
     */
    const RouteEntry* find_route(Method method, const std::string& path) const;

    /**
     * 清除所有路由
     */
    void clear();

    /**
     * 获取路由数量
     */
    size_t count() const { return routes_.size(); }

private:
    std::vector<RouteEntry> routes_;
};

/**
 * HTTP 响应构建器
 */
class ResponseBuilder {
public:
    /**
     * 序列化 HTTP 响应为字节流
     */
    static std::vector<uint8_t> build(const Response& resp);
};

} // namespace http
} // namespace chrono

#endif // CHRONO_CPP_HTTP_PARSER_H
