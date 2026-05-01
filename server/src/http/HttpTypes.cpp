/**
 * Chrono-shift C++ HTTP 类型实现
 */
#include "HttpTypes.h"
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iterator>

namespace chrono {
namespace http {

// ============================================================
// Request 便利函数
// ============================================================

std::string Request::header(const std::string& key) const {
    auto it = headers.find(key);
    if (it != headers.end()) return it->second;

    // 大小写不敏感查找
    std::string lower_key = key;
    std::transform(lower_key.begin(), lower_key.end(), lower_key.begin(), ::tolower);
    for (const auto& [k, v] : headers) {
        std::string kl = k;
        std::transform(kl.begin(), kl.end(), kl.begin(), ::tolower);
        if (kl == lower_key) return v;
    }
    return "";
}

std::string Request::body_string() const {
    return std::string(body.begin(), body.end());
}

// ============================================================
// Response 方法
// ============================================================

Response& Response::set_status(int code, const std::string& text) {
    status_code = code;
    status_text = text;
    return *this;
}

Response& Response::set_header(const std::string& key, const std::string& value) {
    headers[key] = value;
    return *this;
}

Response& Response::set_body(const uint8_t* data, size_t length) {
    body.assign(data, data + length);
    headers["Content-Length"] = std::to_string(length);
    return *this;
}

Response& Response::set_body(const std::string& str) {
    body.assign(str.begin(), str.end());
    headers["Content-Length"] = std::to_string(body.size());
    return *this;
}

Response& Response::set_json(const std::string& json_str) {
    body.assign(json_str.begin(), json_str.end());
    headers["Content-Type"] = "application/json";
    headers["Content-Length"] = std::to_string(body.size());
    return *this;
}

Response& Response::set_file(const std::string& filepath, const std::string& mime_type) {
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file) {
        status_code = 404;
        status_text = "Not Found";
        set_json(R"({"status":"error","message":"File not found"})");
        return *this;
    }

    auto size = file.tellg();
    file.seekg(0, std::ios::beg);

    body.resize(static_cast<size_t>(size));
    file.read(reinterpret_cast<char*>(body.data()), size);

    headers["Content-Type"] = mime_type;
    headers["Content-Length"] = std::to_string(body.size());
    return *this;
}

std::string Response::body_string() const {
    return std::string(body.begin(), body.end());
}

std::string Response::serialize() const {
    std::ostringstream oss;
    oss << "HTTP/1.1 " << status_code << " " << status_text << "\r\n";
    for (const auto& [key, val] : headers) {
        oss << key << ": " << val << "\r\n";
    }
    oss << "\r\n";
    oss << body_string();
    return oss.str();
}

// ============================================================
// 兼容旧 API 的 C 风格辅助函数
// ============================================================

void response_init(Response& resp) {
    resp = Response();
}

void response_set_status(Response& resp, int code, const char* text) {
    resp.set_status(code, text);
}

void response_set_header(Response& resp, const char* key, const char* value) {
    resp.set_header(key, value);
}

void response_set_body(Response& resp, const uint8_t* data, size_t length) {
    resp.set_body(data, length);
}

void response_set_json(Response& resp, const char* json_str) {
    resp.set_json(json_str);
}

void response_set_file(Response& resp, const char* filepath, const char* mime_type) {
    resp.set_file(filepath, mime_type);
}

} // namespace http
} // namespace chrono
