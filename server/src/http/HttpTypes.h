/**
 * Chrono-shift C++ HTTP 类型定义
 * RAII 风格，使用 std::string + std::vector + std::function
 * C++17 重构版
 */
#ifndef CHRONO_CPP_HTTP_TYPES_H
#define CHRONO_CPP_HTTP_TYPES_H

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <cstdint>
#include <memory>

namespace chrono {
namespace http {

// ============================================================
// HTTP 方法
// ============================================================
enum class Method {
    kGet,
    kPost,
    kPut,
    kDelete,
    kPatch,
    kUnknown
};

// 方法字符串转换
inline const char* method_to_string(Method m) {
    switch (m) {
        case Method::kGet:     return "GET";
        case Method::kPost:    return "POST";
        case Method::kPut:     return "PUT";
        case Method::kDelete:  return "DELETE";
        case Method::kPatch:   return "PATCH";
        default:               return "UNKNOWN";
    }
}

inline Method string_to_method(const std::string& s) {
    if (s == "GET")    return Method::kGet;
    if (s == "POST")   return Method::kPost;
    if (s == "PUT")    return Method::kPut;
    if (s == "DELETE") return Method::kDelete;
    if (s == "PATCH")  return Method::kPatch;
    return Method::kUnknown;
}

// ============================================================
// HTTP 请求
// ============================================================
struct Request {
    Method method = Method::kUnknown;
    std::string method_str;
    std::string path;
    std::string query;
    std::string version;
    std::map<std::string, std::string> headers;
    std::vector<uint8_t> body;

    // 便利函数
    std::string header(const std::string& key) const;
    std::string body_string() const;
};

// ============================================================
// HTTP 响应
// ============================================================
struct Response {
    int status_code = 200;
    std::string status_text = "OK";
    std::map<std::string, std::string> headers;
    std::vector<uint8_t> body;

    Response& set_status(int code, const std::string& text);
    Response& set_header(const std::string& key, const std::string& value);
    Response& set_body(const uint8_t* data, size_t length);
    Response& set_body(const std::string& str);
    Response& set_json(const std::string& json_str);
    Response& set_file(const std::string& filepath, const std::string& mime_type);

    std::string body_string() const;

    // 序列化为 wire format (用于调试)
    std::string serialize() const;
};

// ============================================================
// 路由处理函数
// ============================================================
using RouteHandler = std::function<void(const Request&, Response&)>;

// ============================================================
// 常量
// ============================================================
constexpr size_t kMaxRouteCount = 128;
constexpr size_t kMaxHeaderCount = 64;
constexpr size_t kMaxPathLength = 2048;
constexpr size_t kMaxBufferSize = 65536;
constexpr int kListenBacklog = 1024;
constexpr int kTimeoutMs = 30000;
constexpr int kPollIntervalMs = 50;

// ============================================================
// 响应构建辅助 (兼容旧 API)
// ============================================================
void response_init(Response& resp);
void response_set_status(Response& resp, int code, const char* text);
void response_set_header(Response& resp, const char* key, const char* value);
void response_set_body(Response& resp, const uint8_t* data, size_t length);
void response_set_json(Response& resp, const char* json_str);
void response_set_file(Response& resp, const char* filepath, const char* mime_type);

} // namespace http
} // namespace chrono

#endif // CHRONO_CPP_HTTP_TYPES_H
