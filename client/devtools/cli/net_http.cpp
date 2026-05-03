/**
 * net_http.cpp �?开发者模�?CLI HTTP 网络�?(C++23 重构�?
 *
 * 自包含的 HTTP/HTTPS 客户端实�?
 * 支持 TCP 明文�?OpenSSL TLS 加密连接
 *
 * ========== 向后兼容设计 ==========
 * 1) 提供 extern "C" 函数 (http_request / http_get_body / http_get_status)
 *    供现有的 cmd_*.c (C 文件) 调用
 * 2) 提供 C++23 HttpClient RAII 类供�?C++ 代码使用
 * ===================================
 */
#include "devtools_cli.hpp"

namespace cli = chrono::client::cli;
using namespace cli;

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <expected>
#include "print_compat.h"
#include <span>
#include <string>
#include <string_view>
#include <vector>

#ifndef _WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cerrno>
#endif

// ============================================================
// 常量
// ============================================================
static constexpr size_t BUFFER_SIZE = 65536;

// ============================================================
// 平台抽象
// ============================================================
#ifdef _WIN32
using socket_t = SOCKET;
constexpr auto INVALID_SOCKET_VALUE = INVALID_SOCKET;
#define ISVALIDSOCKET(s) ((s) != INVALID_SOCKET)
#define CLOSE_SOCKET(s)  closesocket(s)
#else
using socket_t = int;
constexpr auto INVALID_SOCKET_VALUE = socket_t{-1};
#define ISVALIDSOCKET(s) ((s) >= 0)
#define CLOSE_SOCKET(s)  close(s)
#endif

// ============================================================
// TLS 外部 C 函数声明
// ============================================================
extern "C" {
    extern int  tls_client_init(const char* cert_dir);
    extern int  tls_client_connect(void** ssl, const char* host,
                                    unsigned short port);
    extern int  tls_write(void* ssl, const char* data, size_t len);
    extern int  tls_read(void* ssl, char* buf, size_t len);
    extern void tls_close(void* ssl);
    extern const char* tls_last_error(void);
}

// ============================================================
// 内部工具函数
// ============================================================

/** 建立 TCP 连接 */
static socket_t tcp_connect(const std::string& host, int port)
{
    struct hostent* he = gethostbyname(host.c_str());
    if (!he) {
        cli::println(stderr, "[-] 无法解析主机: {}", host);
        return INVALID_SOCKET_VALUE;
    }

    socket_t fd = static_cast<socket_t>(::socket(AF_INET, SOCK_STREAM, 0));
    if (!ISVALIDSOCKET(fd)) {
        cli::println(stderr, "[-] 创建 socket 失败");
        return INVALID_SOCKET_VALUE;
    }

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<unsigned short>(port));
    std::memcpy(&addr.sin_addr, he->h_addr_list[0],
                static_cast<size_t>(he->h_length));

    if (::connect(fd, reinterpret_cast<struct sockaddr*>(&addr),
                  sizeof(addr)) != 0) {
        cli::println(stderr, "[-] 连接 {}:{} 失败", host, port);
        CLOSE_SOCKET(fd);
        return INVALID_SOCKET_VALUE;
    }

    return fd;
}

/** 发送所有数�?*/
static bool send_all(socket_t fd, std::span<const char> data)
{
    size_t sent = 0;
    while (sent < data.size()) {
#ifdef _WIN32
        int n = ::send(fd, data.data() + sent,
                       static_cast<int>(data.size() - sent), 0);
#else
        auto n = ::send(fd, data.data() + sent,
                        data.size() - sent, 0);
#endif
        if (n <= 0) {
            cli::println(stderr, "[-] 发送请求失败");
            return false;
        }
        sent += static_cast<size_t>(n);
    }
    return true;
}

/** 接收所有响应数�?*/
static std::string recv_all(socket_t fd)
{
    std::string response;
    response.resize(BUFFER_SIZE);
    size_t total = 0;

    while (total < BUFFER_SIZE - 1) {
#ifdef _WIN32
        int n = ::recv(fd, response.data() + total,
                       static_cast<int>(BUFFER_SIZE - 1 - total), 0);
#else
        auto n = ::recv(fd, response.data() + total,
                        BUFFER_SIZE - 1 - total, 0);
#endif
        if (n <= 0) break;
        total += static_cast<size_t>(n);
    }
    response.resize(total);
    return response;
}

// ============================================================
// parse_response 前向声明 �?HttpClient::request() 中使�?
// ============================================================
static auto parse_response(std::string raw)
    -> std::expected<HttpClient::Response, std::string>;

// ============================================================
// extern "C" 兼容�?�?�?cmd_*.c (C 文件) 调用
// ============================================================

extern "C" int http_request(
    const char* method,
    const char* path,
    const char* body,
    const char* content_type,
    char* response,
    size_t resp_size)
{
    // 使用 C++23 内部�?request 逻辑
    auto& cfg = chrono::client::cli::g_cli_config;
    std::string request_buf;
    std::string body_str = body ? body : "";
    std::string ct = content_type ? content_type : "application/json";

    if (!body_str.empty()) {
        request_buf = std::format(
            "{} {} HTTP/1.1\r\n"
            "Host: {}:{}\r\n"
            "Content-Type: {}\r\n"
            "Content-Length: {}\r\n"
            "Connection: close\r\n"
            "\r\n"
            "{}",
            method, path,
            cfg.host, cfg.port,
            ct,
            body_str.size(),
            body_str);
    } else {
        request_buf = std::format(
            "{} {} HTTP/1.1\r\n"
            "Host: {}:{}\r\n"
            "Connection: close\r\n"
            "\r\n",
            method, path,
            cfg.host, cfg.port);
    }

    if (cfg.verbose) {
        cli::println("[*] 发送请求:\n{}", request_buf);
    }

    if (cfg.use_tls) {
        // === HTTPS 模式: TLS ===
        void* ssl = nullptr;
        if (tls_client_init(nullptr) != 0) {
            cli::println(stderr, "[-] TLS 客户端初始化失败: {}",
                         tls_last_error());
            return -1;
        }
        if (tls_client_connect(&ssl, cfg.host.c_str(),
                               static_cast<unsigned short>(cfg.port)) != 0) {
            cli::println(stderr, "[-] TLS 连接 {}:{} 失败: {}",
                         cfg.host, cfg.port, tls_last_error());
            return -1;
        }

        // 发�?
        int sent = 0;
        int len = static_cast<int>(request_buf.size());
        while (sent < len) {
            int n = tls_write(ssl, request_buf.data() + sent,
                              static_cast<size_t>(len - sent));
            if (n < 0) {
                cli::println(stderr, "[-] TLS 发送失败: {}",
                             tls_last_error());
                tls_close(ssl);
                return -1;
            }
            sent += n;
        }

        // 接收
        size_t total = 0;
        int n;
        while (total < resp_size - 1) {
            n = tls_read(ssl, response + total, resp_size - 1 - total);
            if (n < 0) {
                cli::println(stderr, "[-] TLS 接收失败: {}",
                             tls_last_error());
                tls_close(ssl);
                return -1;
            }
            if (n == 0) break;
            total += static_cast<size_t>(n);
        }
        response[total] = '\0';

        tls_close(ssl);

        if (total == 0) {
            cli::println(stderr, "[-] 未收到响应");
            return -1;
        }
        return 0;

    } else {
        // === HTTP 模式: 原始 TCP ===
        socket_t fd = tcp_connect(cfg.host, cfg.port);
        if (!ISVALIDSOCKET(fd)) return -1;

        if (!send_all(fd, std::span<const char>(
                request_buf.data(), request_buf.size()))) {
            CLOSE_SOCKET(fd);
            return -1;
        }

        std::string resp = recv_all(fd);
        CLOSE_SOCKET(fd);

        if (resp.empty()) {
            cli::println(stderr, "[-] 未收到响应");
            return -1;
        }

        // 拷贝�?C 风格输出缓冲�?
        size_t copy_len = std::min(resp.size(), resp_size - 1);
        std::memcpy(response, resp.data(), copy_len);
        response[copy_len] = '\0';
        return 0;
    }
}

extern "C" const char* http_get_body(const char* response)
{
    if (!response) return response;
    const char* body = std::strstr(response, "\r\n\r\n");
    if (body) return body + 4;
    return response;
}

extern "C" int http_get_status(const char* response)
{
    if (!response) return -1;
    int code = 0;
    if (std::sscanf(response, "HTTP/1.%*d %d", &code) == 1 ||
        std::sscanf(response, "HTTP/%*d.%*d %d", &code) == 1) {
        return code;
    }
    return -1;
}

// ============================================================
// HttpClient::request �?C++23 RAII 实现
// ============================================================

auto chrono::client::cli::HttpClient::request(
    std::string_view method,
    std::string_view path,
    std::string_view body,
    std::string_view content_type) -> std::expected<Response, std::string>
{
    std::string request_buf;

    if (!body.empty()) {
        request_buf = std::format(
            "{} {} HTTP/1.1\r\n"
            "Host: {}:{}\r\n"
            "Content-Type: {}\r\n"
            "Content-Length: {}\r\n"
            "Connection: close\r\n"
            "\r\n"
            "{}",
            method, path,
            config_.host, config_.port,
            content_type.empty() ? "application/json" : content_type,
            body.size(),
            body);
    } else {
        request_buf = std::format(
            "{} {} HTTP/1.1\r\n"
            "Host: {}:{}\r\n"
            "Connection: close\r\n"
            "\r\n",
            method, path,
            config_.host, config_.port);
    }

    if (config_.verbose) {
        cli::println("[*] 发送请求:\n{}", request_buf);
    }

    if (config_.use_tls) {
        TlsRaii tls;
        if (!tls.connect(config_.host, config_.port)) {
            return std::unexpected(
                std::format("TLS 连接失败: {}", tls.last_error()));
        }

        // 发�?
        int sent = 0;
        int len = static_cast<int>(request_buf.size());
        while (sent < len) {
            int n = tls.write(std::span<const char>(
                request_buf.data() + sent, static_cast<size_t>(len - sent)));
            if (n < 0) {
                return std::unexpected(
                    std::format("TLS 发送失败: {}", tls.last_error()));
            }
            sent += n;
        }

        // 接收
        std::string raw;
        raw.resize(BUFFER_SIZE);
        size_t total = 0;
        int n;
        while (total < BUFFER_SIZE - 1) {
            n = tls.read(std::span<char>(
                raw.data() + total, BUFFER_SIZE - 1 - total));
            if (n < 0) {
                return std::unexpected(
                    std::format("TLS 接收失败: {}", tls.last_error()));
            }
            if (n == 0) break;
            total += static_cast<size_t>(n);
        }
        raw.resize(total);

        if (raw.empty()) {
            return std::unexpected("未收到响应");
        }

        return parse_response(std::move(raw));

    } else {
        // TCP 明文
        socket_t fd = tcp_connect(config_.host, config_.port);
        if (!ISVALIDSOCKET(fd)) {
            return std::unexpected(
                std::format("TCP 连接 {}:{} 失败",
                            config_.host, config_.port));
        }

        if (!send_all(fd, std::span<const char>(
                request_buf.data(), request_buf.size()))) {
            CLOSE_SOCKET(fd);
            return std::unexpected("发送请求失败");
        }

        std::string raw = recv_all(fd);
        CLOSE_SOCKET(fd);

        if (raw.empty()) {
            return std::unexpected("未收到响应");
        }

        return parse_response(std::move(raw));
    }
}

/** 解析 HTTP 响应 */
static auto parse_response(std::string raw) -> std::expected<HttpClient::Response, std::string>
{
    HttpClient::Response resp;
    resp.raw = std::move(raw);

    // 解析状态码
    if (std::sscanf(resp.raw.c_str(), "HTTP/%*d.%*d %d", &resp.status_code) != 1) {
        return std::unexpected("无法解析 HTTP 响应");
    }

    // 提取 body
    auto body_pos = resp.raw.find("\r\n\r\n");
    if (body_pos != std::string::npos) {
        resp.body = resp.raw.substr(body_pos + 4);
    }

    return resp;
}
