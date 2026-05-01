/**
 * Chrono-shift C++ HTTP 解析器实现
 */
#include "HttpParser.h"
#include "../util/StringUtils.h"
#include <cstring>
#include <cstdlib>
#include <sstream>
#include <algorithm>

namespace chrono {
namespace http {

// ============================================================
// HttpParser 实现
// ============================================================

std::optional<Request> HttpParser::parse(const uint8_t* data, size_t length) {
    if (!data || length == 0) {
        error_ = "empty data";
        return std::nullopt;
    }
    return parse(std::string(reinterpret_cast<const char*>(data), length));
}

std::optional<Request> HttpParser::parse(const std::string& data) {
    error_.clear();
    if (data.empty()) {
        error_ = "empty request";
        return std::nullopt;
    }

    Request req;

    // 查找头部结束标志
    auto header_end = data.find("\r\n\r\n");
    if (header_end == std::string::npos) {
        error_ = "incomplete headers";
        return std::nullopt;
    }

    // 解析请求行 + 头部
    std::string header_section = data.substr(0, header_end);
    std::istringstream stream(header_section);
    std::string line;

    // 请求行
    if (!std::getline(stream, line)) {
        error_ = "empty request line";
        return std::nullopt;
    }
    // 去掉 \r
    if (!line.empty() && line.back() == '\r') line.pop_back();

    if (!parse_request_line(req, line)) {
        return std::nullopt;
    }

    // 头部
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) break;
        if (!parse_header_line(req, line)) {
            // 非致命错误，继续解析
            continue;
        }
    }

    // 解析 body (如果有 Content-Length)
    auto it = req.headers.find("Content-Length");
    if (it != req.headers.end()) {
        size_t content_length = std::stoul(it->second);
        size_t body_start = header_end + 4; // 跳过 \r\n\r\n

        if (body_start + content_length <= data.size()) {
            req.body.assign(
                data.begin() + static_cast<ptrdiff_t>(body_start),
                data.begin() + static_cast<ptrdiff_t>(body_start + content_length));
        }
    }

    return req;
}

bool HttpParser::parse_request_line(Request& req, const std::string& line) {
    // 格式: METHOD PATH HTTP/1.1
    auto first_space = line.find(' ');
    if (first_space == std::string::npos) {
        error_ = "invalid request line (no method)";
        return false;
    }

    std::string method_str = line.substr(0, first_space);
    req.method_str = method_str;
    req.method = string_to_method(method_str);

    auto second_space = line.find(' ', first_space + 1);
    if (second_space == std::string::npos) {
        error_ = "invalid request line (no path)";
        return false;
    }

    std::string path_and_query = line.substr(first_space + 1,
                                              second_space - first_space - 1);

    // 分离 path 和 query
    auto query_pos = path_and_query.find('?');
    if (query_pos != std::string::npos) {
        req.path = path_and_query.substr(0, query_pos);
        req.query = path_and_query.substr(query_pos + 1);
    } else {
        req.path = path_and_query;
    }

    req.version = line.substr(second_space + 1);

    return true;
}

bool HttpParser::parse_header_line(Request& req, const std::string& line) {
    auto colon_pos = line.find(':');
    if (colon_pos == std::string::npos) {
        return false;
    }

    std::string key = line.substr(0, colon_pos);
    std::string value;

    // 跳过 key 后的空白
    auto val_start = colon_pos + 1;
    while (val_start < line.size() && line[val_start] == ' ') {
        val_start++;
    }
    value = line.substr(val_start);

    req.headers[key] = value;
    return true;
}

// ============================================================
// Router 实现
// ============================================================

bool Router::add_route(Method method, const std::string& path, RouteHandler handler) {
    if (routes_.size() >= kMaxRouteCount) {
        return false;
    }
    routes_.push_back({method, path, std::move(handler)});
    return true;
}

const RouteEntry* Router::find_route(Method method, const std::string& path) const {
    for (const auto& route : routes_) {
        if (route.method == method && route.path == path) {
            return &route;
        }
    }
    return nullptr;
}

void Router::clear() {
    routes_.clear();
}

// ============================================================
// ResponseBuilder 实现
// ============================================================

std::vector<uint8_t> ResponseBuilder::build(const Response& resp) {
    std::string header_str;

    // 状态行
    header_str += "HTTP/1.1 " + std::to_string(resp.status_code) + " " +
                  resp.status_text + "\r\n";

    // 头部
    for (const auto& [key, val] : resp.headers) {
        header_str += key + ": " + val + "\r\n";
    }

    // 空行
    header_str += "\r\n";

    // 合并头部 + body
    std::vector<uint8_t> result(header_str.begin(), header_str.end());
    result.insert(result.end(), resp.body.begin(), resp.body.end());

    return result;
}

} // namespace http
} // namespace chrono
