/**
 * Chrono-shift 客户端 HTTP 连接实现
 * C++17 重构版 (从 C99 net_http.c 移植)
 */
#include "HttpConnection.h"
#include "TcpConnection.h"

#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#else
#include <unistd.h>
#include <sys/socket.h>
#endif

#include "../../server/include/tls_server.h"

#include "../util/Logger.h"

namespace chrono {
namespace client {
namespace network {

namespace {
    constexpr size_t kNetBufSize = 65536;
    constexpr const char* kHttpProto = "HTTP/1.1";
}

HttpConnection::HttpConnection(TcpConnection& tcp)
    : tcp_(tcp)
{
}

HttpConnection::Response HttpConnection::request(
    const std::string& method,
    const std::string& path,
    const std::string& headers,
    const uint8_t* body,
    size_t body_len)
{
    Response resp;

    // 如果未连接，尝试自动重连
    if (!tcp_.is_connected()) {
        if (!tcp_.reconnect()) {
            LOG_ERROR("HTTP 请求失败: 未连接且重连失败");
            return resp;
        }
    }

    // 构建 HTTP 请求
    std::string request;
    request.reserve(4096);

    request += method + " " + path + " " + kHttpProto + "\r\n";
    request += "Host: " + tcp_.host() + "\r\n";

    if (body && body_len > 0) {
        request += "Content-Length: " + std::to_string(body_len) + "\r\n";
    }

    if (!headers.empty()) {
        request += headers;
        if (headers.back() != '\n') {
            request += "\r\n";
        }
    }

    request += "\r\n";

    // 发送请求 (带一次重试)
    for (int attempt = 0; attempt < 2; attempt++) {
        if (attempt > 0) {
            // 第一次失败后重连重试
            if (!tcp_.reconnect()) {
                return resp;
            }
        }

        // 发送请求头
        if (tcp_.send_all(
                reinterpret_cast<const uint8_t*>(request.data()),
                request.size()) != 0) {
            LOG_WARN("发送 HTTP 请求失败 (尝试 %d/2)", attempt + 1);
            continue;
        }

        // 发送请求体
        if (body && body_len > 0) {
            if (tcp_.send_all(body, body_len) != 0) {
                LOG_WARN("发送 HTTP body 失败 (尝试 %d/2)", attempt + 1);
                continue;
            }
        }

        // --- 接收响应 ---
        std::vector<uint8_t> buffer(kNetBufSize);
        size_t buf_pos = 0;
        bool in_body = false;
        size_t content_length = 0;
        size_t body_received = 0;
        const char* body_start = nullptr;
        bool recv_ok = true;

        while (buf_pos < buffer.size() - 1) {
            int n_recv;
            if (tcp_.get_ssl()) {
                n_recv = static_cast<int>(
                    tls_read(static_cast<SSL*>(tcp_.get_ssl()),
                             buffer.data() + buf_pos,
                             static_cast<int>(buffer.size() - buf_pos - 1)));
            } else {
#ifdef _WIN32
                n_recv = recv(tcp_.get_socket(),
                              reinterpret_cast<char*>(buffer.data() + buf_pos),
                              static_cast<int>(buffer.size() - buf_pos - 1), 0);
#else
                n_recv = static_cast<int>(
                    read(tcp_.get_socket(),
                         buffer.data() + buf_pos,
                         buffer.size() - buf_pos - 1));
#endif
            }
            if (n_recv <= 0) {
                recv_ok = false;
                break;
            }
            buf_pos += static_cast<size_t>(n_recv);
            buffer[buf_pos] = '\0';

            if (!in_body) {
                // 查找 \r\n\r\n 分隔符
                const char* header_end = std::strstr(
                    reinterpret_cast<const char*>(buffer.data()), "\r\n\r\n");
                if (header_end) {
                    in_body = true;
                    body_start = header_end + 4;
                    body_received = buf_pos -
                        static_cast<size_t>(body_start -
                            reinterpret_cast<const char*>(buffer.data()));

                    // 解析 Content-Length
                    const char* cl = std::strstr(
                        reinterpret_cast<const char*>(buffer.data()),
                        "Content-Length:");
                    if (cl) {
                        cl += 16;
                        while (*cl == ' ') cl++;
                        content_length = static_cast<size_t>(std::atol(cl));
                    } else {
                        content_length = buf_pos;
                    }

                    // 解析状态码
                    const char* status_start =
                        reinterpret_cast<const char*>(buffer.data());
                    // 跳过 "HTTP/1.1 "
                    const char* sp = std::strchr(status_start, ' ');
                    if (sp) {
                        sp++;
                        resp.status_code = std::atoi(sp);
                        // 获取状态文本
                        const char* sp2 = std::strchr(sp, ' ');
                        if (sp2) {
                            sp2++;
                            const char* eol = std::strstr(sp2, "\r\n");
                            if (eol) {
                                resp.status_text.assign(sp2, eol);
                            }
                        }
                    }
                }
            }

            if (in_body) {
                if (content_length > 0 && body_received >= content_length) {
                    break;
                }
            }
        }

        if (!in_body) {
            LOG_WARN("HTTP 响应不完整 (尝试 %d/2)", attempt + 1);
            continue;
        }

        if (!recv_ok && body_received == 0) {
            LOG_WARN("HTTP 接收失败 (尝试 %d/2)", attempt + 1);
            continue;
        }

        // 提取 body
        size_t actual_body_len = body_received;
        if (content_length > 0 && body_received > content_length) {
            actual_body_len = content_length;
        } else if (content_length > 0) {
            actual_body_len = content_length;
        }

        if (body_start && actual_body_len > 0) {
            resp.body.assign(body_start, actual_body_len);
        }

        // 解析响应头 (可扩展)
        const char* header_start =
            reinterpret_cast<const char*>(buffer.data());
        const char* header_end = std::strstr(header_start, "\r\n\r\n");
        if (header_end) {
            // 跳状态行
            const char* line = std::strchr(header_start, '\n');
            while (line && line < header_end) {
                line++;
                const char* next_line = std::strchr(line, '\n');
                if (!next_line || next_line > header_end) {
                    next_line = header_end;
                }
                std::string header_line(line, next_line - line);
                // 去除尾部 \r
                if (!header_line.empty() && header_line.back() == '\r') {
                    header_line.pop_back();
                }
                auto colon = header_line.find(':');
                if (colon != std::string::npos) {
                    std::string key = header_line.substr(0, colon);
                    std::string value = header_line.substr(colon + 2);
                    // 去除尾部空白
                    while (!value.empty() &&
                           (value.back() == ' ' || value.back() == '\r')) {
                        value.pop_back();
                    }
                    resp.headers[std::move(key)] = std::move(value);
                }
                line = next_line;
            }
        }

        return resp;
    }

    return resp;
}

} // namespace network
} // namespace client
} // namespace chrono
