/**
 * devtools_cli.hpp — 开发者模式 CLI C++23 核心头文件
 *
 * C++23 重构版本: namespace、RAII、std::expected、std::move_only_function
 * 逐步替代 devtools_cli.h (C 风格头文件)
 *
 * 与旧 C 头文件共存: cmd_*.c 仍使用 devtools_cli.h,
 * 新 C++ 代码使用此头文件。
 */
#pragma once

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <expected>
#include <functional>
#include <memory>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <cerrno>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

// ============================================================
// 命名空间
// ============================================================
namespace chrono::client::cli {

// ============================================================
// 类型别名
// ============================================================
using Args = std::span<const std::string_view>;
using CommandHandler = std::move_only_function<int(Args)>;

// ============================================================
// 命令条目
// ============================================================
struct Command {
    std::string name;
    std::string description;
    std::string usage;
    CommandHandler handler;
};

// ============================================================
// 命令注册表 (替代 C 风格 g_command_table[] + register_command())
// ============================================================
class CommandRegistry {
public:
    void add(std::string name, std::string desc, std::string usage,
             CommandHandler handler)
    {
        commands_.push_back(Command{
            .name = std::move(name),
            .description = std::move(desc),
            .usage = std::move(usage),
            .handler = std::move(handler),
        });
    }

    CommandHandler* find(std::string_view name) noexcept
    {
        for (auto& cmd : commands_) {
            if (cmd.name == name) {
                return &cmd.handler;
            }
        }
        return nullptr;
    }

    [[nodiscard]] std::span<const Command> all() const noexcept
    {
        return commands_;
    }

    [[nodiscard]] size_t size() const noexcept { return commands_.size(); }

private:
    std::vector<Command> commands_;
};

// ============================================================
// 全局配置 (替代 C 风格 DevToolsConfig + g_config)
// ============================================================
struct Config {
    // 服务器连接
    std::string host = "127.0.0.1";
    uint16_t port = 4443;
    bool use_tls = true;

    // 会话
    bool session_logged_in = false;
    std::string session_token;
    std::string session_host;

    // WebSocket
    bool ws_connected = false;

    // 调试
    bool verbose = false;
    std::string storage_path = "./data";

    // 从环境变量加载配置
    void load_from_env() noexcept
    {
        if (auto* env = std::getenv("CHRONO_HOST")) {
            host = env;
        }
        if (auto* env = std::getenv("CHRONO_PORT")) {
            port = static_cast<uint16_t>(std::atoi(env));
        }
        if (auto* env = std::getenv("CHRONO_TLS")) {
            use_tls = (std::atoi(env) != 0);
        }
    }
};

// ============================================================
// TLS RAII 包装 (替代 void* ws_ssl 裸指针)
// ============================================================
class TlsRaii {
public:
    TlsRaii() noexcept = default;

    ~TlsRaii() { close(); }

    TlsRaii(TlsRaii&& other) noexcept : ssl_(other.ssl_)
    {
        other.ssl_ = nullptr;
    }

    TlsRaii& operator=(TlsRaii&& other) noexcept
    {
        if (this != &other) {
            close();
            ssl_ = other.ssl_;
            other.ssl_ = nullptr;
        }
        return *this;
    }

    TlsRaii(const TlsRaii&) = delete;
    TlsRaii& operator=(const TlsRaii&) = delete;

    /** 建立 TLS 连接 */
    [[nodiscard]] bool connect(const std::string& host, uint16_t port)
    {
        close();
        void* ssl = nullptr;
        if (tls_client_init(nullptr) != 0) {
            return false;
        }
        if (tls_client_connect(&ssl, host.c_str(), port) != 0) {
            tls_close(ssl);
            return false;
        }
        ssl_ = ssl;
        return true;
    }

    /** 写入数据 */
    int write(std::span<const char> data)
    {
        if (!ssl_) return -1;
        return tls_write(ssl_, data.data(), data.size());
    }

    /** 读取数据 */
    int read(std::span<char> buf)
    {
        if (!ssl_) return -1;
        return tls_read(ssl_, buf.data(), buf.size());
    }

    /** 关闭连接 */
    void close() noexcept
    {
        if (ssl_) {
            tls_close(ssl_);
            ssl_ = nullptr;
        }
    }

    explicit operator bool() const noexcept { return ssl_ != nullptr; }

    /** 获取最后错误 */
    std::string_view last_error() const noexcept
    {
        return tls_last_error();
    }

private:
    void* ssl_ = nullptr;

    // TLS 外部 C 函数
    extern "C" int  tls_client_init(const char* cert_dir);
    extern "C" int  tls_client_connect(void** ssl, const char* host,
                                        unsigned short port);
    extern "C" int  tls_write(void* ssl, const char* data, size_t len);
    extern "C" int  tls_read(void* ssl, char* buf, size_t len);
    extern "C" void tls_close(void* ssl);
    extern "C" const char* tls_last_error(void);
};

// ============================================================
// HTTP 客户端 (RAII)
// ============================================================
class HttpClient {
public:
    struct Response {
        int status_code = 0;
        std::string body;
        std::string raw;

        [[nodiscard]] bool ok() const noexcept
        {
            return status_code >= 200 && status_code < 300;
        }
    };

    explicit HttpClient(const Config& cfg) noexcept : config_(cfg) {}

    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;
    HttpClient(HttpClient&&) = delete;
    HttpClient& operator=(HttpClient&&) = delete;

    /** GET 请求 */
    std::expected<Response, std::string>
    get(std::string_view path)
    {
        return request("GET", path, {}, {});
    }

    /** POST 请求 */
    std::expected<Response, std::string>
    post(std::string_view path, std::string_view body = {},
         std::string_view content_type = "application/json")
    {
        return request("POST", path, body, content_type);
    }

    /** PUT 请求 */
    std::expected<Response, std::string>
    put(std::string_view path, std::string_view body = {},
        std::string_view content_type = "application/json")
    {
        return request("PUT", path, body, content_type);
    }

    /** DELETE 请求 */
    std::expected<Response, std::string>
    del(std::string_view path)
    {
        return request("DELETE", path, {}, {});
    }

private:
    const Config& config_;

    std::expected<Response, std::string>
    request(std::string_view method, std::string_view path,
            std::string_view body, std::string_view content_type);
};

// ============================================================
// 工具函数
// ============================================================

/** 获取当前时间戳字符串 (HH:MM:SS) */
std::string timestamp_str();

/** 打印带颜色的文本 */
void print_colored(std::string_view color, std::string_view text);

/** JSON 格式化输出 (缩进) */
void print_json(std::string_view json, int indent = 4);

/** Base64 解码 */
auto base64_decode(std::string_view in)
    -> std::expected<std::vector<std::uint8_t>, std::string>;

/** 从 HTTP 响应中提取消息体 */
std::string_view http_get_body(std::string_view response);

/** 解析 HTTP 响应状态码 */
int http_get_status(std::string_view response);

// ============================================================
// 颜色常量
// ============================================================
inline constexpr auto COLOR_RESET  = "\033[0m"sv;
inline constexpr auto COLOR_RED    = "\033[31m"sv;
inline constexpr auto COLOR_GREEN  = "\033[32m"sv;
inline constexpr auto COLOR_YELLOW = "\033[33m"sv;
inline constexpr auto COLOR_BLUE   = "\033[34m"sv;
inline constexpr auto COLOR_CYAN   = "\033[36m"sv;
inline constexpr auto COLOR_BOLD   = "\033[1m"sv;

// ============================================================
// 全局实例声明 (定义在 main.cpp)
// ============================================================
extern CommandRegistry g_command_registry;
extern Config g_cli_config;

} // namespace chrono::client::cli
