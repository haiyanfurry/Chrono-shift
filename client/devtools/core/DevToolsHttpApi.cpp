/**
 * DevToolsHttpApi.cpp — HTTP API 路由处理器实现
 *
 * 注册到 ClientHttpServer 的 /api/devtools/ 路由前缀，
 * 将 HTTP 请求转换为 DevToolsEngine 命令调用。
 *
 * 注意：包含路径在 CMake 中通过 include_directories 配置，
 * 实际构建时需添加：
 *   ${CMAKE_CURRENT_SOURCE_DIR}/src
 *   ${CMAKE_CURRENT_SOURCE_DIR}/devtools/cli
 */
#include "DevToolsHttpApi.h"
#include "DevToolsEngine.h"

#include "devtools_cli.h"          // g_config, g_command_table
#include "app/ClientHttpServer.h"

#include <sstream>
#include <cstring>

namespace chrono {
namespace client {
namespace devtools {

/* ============================================================
 * 构造函数
 * ============================================================ */

DevToolsHttpApi::DevToolsHttpApi(DevToolsEngine& engine)
    : engine_(engine)
{
}

/* ============================================================
 * 路由注册
 * ============================================================ */

void DevToolsHttpApi::register_routes(app::ClientHttpServer& http_server)
{
    using namespace app;

    http_server.register_route(
        ClientHttpServer::kDevToolRoutePrefix,
        [this](SOCKET fd, const std::string& path,
               const std::string& method, const std::string& body)
        {
            this->handle_request(fd, path, method, body);
        });
}

void DevToolsHttpApi::unregister_routes(app::ClientHttpServer& http_server)
{
    http_server.unregister_route(
        app::ClientHttpServer::kDevToolRoutePrefix);
}

/* ============================================================
 * 请求分发
 * ============================================================ */

void DevToolsHttpApi::handle_request(SOCKET fd, const std::string& path,
                                     const std::string& method,
                                     const std::string& body)
{
    /* 提取子路径 */
    std::string prefix = app::ClientHttpServer::kDevToolRoutePrefix;
    std::string sub = path.substr(prefix.size());

    /* 去除尾部斜杠 */
    while (!sub.empty() && sub.back() == '/') {
        sub.pop_back();
    }

    if (sub == "exec") {
        handle_exec(fd, body);
    } else if (sub == "commands") {
        handle_commands(fd);
    } else if (sub == "status") {
        handle_status(fd);
    } else if (sub == "enable") {
        handle_enable(fd);
    } else if (sub == "disable") {
        handle_disable(fd);
    } else if (sub == "network/status") {
        handle_network_status(fd);
    } else if (sub == "storage/list") {
        handle_storage_list(fd);
    } else if (sub == "endpoint/test") {
        handle_endpoint_test(fd, body);
    } else if (sub == "ws/status") {
        handle_ws_status(fd);
    } else if (sub == "ws/send") {
        handle_ws_send(fd, body);
    } else {
        /* 回退到引擎的 execute_command */
        std::string output = engine_.execute_command(sub);
        std::string json = "{\"status\":\"ok\",\"output\":\""
                         + escape_json(output) + "\"}";
        /* 直接通过 HTTP 发送响应 */
        std::string response =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: " + std::to_string(json.size()) + "\r\n"
            "Connection: close\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "\r\n" + json;
        ::send(fd, response.data(), static_cast<int>(response.size()), 0);
    }
}

/* ============================================================
 * 端点处理器
 * ============================================================ */

void DevToolsHttpApi::handle_exec(SOCKET fd, const std::string& body)
{
    /* 提取 "cmd" 字段 (简单 JSON 解析) */
    std::string cmd;
    auto pos = body.find("\"cmd\"");
    if (pos != std::string::npos) {
        auto vstart = body.find('"', pos + 5);
        if (vstart != std::string::npos) {
            auto vend = body.find('"', vstart + 1);
            if (vend != std::string::npos) {
                cmd = body.substr(vstart + 1, vend - vstart - 1);
            }
        }
    }

    if (cmd.empty()) {
        std::string err = "{\"status\":\"error\",\"message\":\"Missing 'cmd' field\"}";
        std::string response =
            "HTTP/1.1 400 Bad Request\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: " + std::to_string(err.size()) + "\r\n"
            "Connection: close\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "\r\n" + err;
        ::send(fd, response.data(), static_cast<int>(response.size()), 0);
        return;
    }

    std::string output = engine_.execute_command(cmd);
    std::string json = "{\"status\":\"ok\",\"cmd\":\""
                     + escape_json(cmd) + "\",\"output\":\""
                     + escape_json(output) + "\"}";

    std::string response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: " + std::to_string(json.size()) + "\r\n"
        "Connection: close\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "\r\n" + json;
    ::send(fd, response.data(), static_cast<int>(response.size()), 0);
}

void DevToolsHttpApi::handle_commands(SOCKET fd)
{
    std::string json = engine_.get_commands_json();
    std::string response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: " + std::to_string(json.size()) + "\r\n"
        "Connection: close\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "\r\n" + json;
    ::send(fd, response.data(), static_cast<int>(response.size()), 0);
}

void DevToolsHttpApi::handle_status(SOCKET fd)
{
    std::string json = engine_.get_status_json();
    std::string response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: " + std::to_string(json.size()) + "\r\n"
        "Connection: close\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "\r\n" + json;
    ::send(fd, response.data(), static_cast<int>(response.size()), 0);
}

void DevToolsHttpApi::handle_enable(SOCKET fd)
{
    engine_.set_dev_mode_enabled(true);
    std::string json = "{\"status\":\"ok\",\"enabled\":true}";
    std::string response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: " + std::to_string(json.size()) + "\r\n"
        "Connection: close\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "\r\n" + json;
    ::send(fd, response.data(), static_cast<int>(response.size()), 0);
}

void DevToolsHttpApi::handle_disable(SOCKET fd)
{
    engine_.set_dev_mode_enabled(false);
    std::string json = "{\"status\":\"ok\",\"enabled\":false}";
    std::string response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: " + std::to_string(json.size()) + "\r\n"
        "Connection: close\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "\r\n" + json;
    ::send(fd, response.data(), static_cast<int>(response.size()), 0);
}

void DevToolsHttpApi::handle_network_status(SOCKET fd)
{
    std::ostringstream json;
    json << "{"
         << "\"status\":\"ok\","
         << "\"host\":\"" << g_config.host << "\","
         << "\"port\":" << g_config.port << ","
         << "\"use_tls\":" << (g_config.use_tls ? "true" : "false") << ","
         << "\"ws_connected\":" << (g_config.ws_connected ? "true" : "false") << ","
         << "\"logged_in\":" << (g_config.session_logged_in ? "true" : "false")
         << "}";

    std::string json_str = json.str();
    std::string response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: " + std::to_string(json_str.size()) + "\r\n"
        "Connection: close\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "\r\n" + json_str;
    ::send(fd, response.data(), static_cast<int>(response.size()), 0);
}

void DevToolsHttpApi::handle_storage_list(SOCKET fd)
{
    std::string json =
        "{\"status\":\"ok\",\"keys\":[],\"path\":\""
        + escape_json(g_config.storage_path) + "\"}";

    std::string response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: " + std::to_string(json.size()) + "\r\n"
        "Connection: close\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "\r\n" + json;
    ::send(fd, response.data(), static_cast<int>(response.size()), 0);
}

void DevToolsHttpApi::handle_endpoint_test(SOCKET fd, const std::string& body)
{
    std::string output = engine_.execute_command("endpoint " + body);
    std::string json = "{\"status\":\"ok\",\"output\":\""
                     + escape_json(output) + "\"}";

    std::string response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: " + std::to_string(json.size()) + "\r\n"
        "Connection: close\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "\r\n" + json;
    ::send(fd, response.data(), static_cast<int>(response.size()), 0);
}

void DevToolsHttpApi::handle_ws_status(SOCKET fd)
{
    std::string json = "{\"status\":\"ok\",\"ws_connected\":"
                     + std::string(g_config.ws_connected ? "true" : "false")
                     + "}";

    std::string response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: " + std::to_string(json.size()) + "\r\n"
        "Connection: close\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "\r\n" + json;
    ::send(fd, response.data(), static_cast<int>(response.size()), 0);
}

void DevToolsHttpApi::handle_ws_send(SOCKET fd, const std::string& body)
{
    std::string output = engine_.execute_command("ws send " + body);
    std::string json = "{\"status\":\"ok\",\"output\":\""
                     + escape_json(output) + "\"}";

    std::string response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: " + std::to_string(json.size()) + "\r\n"
        "Connection: close\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "\r\n" + json;
    ::send(fd, response.data(), static_cast<int>(response.size()), 0);
}

/* ============================================================
 * 工具
 * ============================================================ */

std::string DevToolsHttpApi::escape_json(const std::string& raw)
{
    std::string out;
    for (char c : raw) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x",
                             static_cast<unsigned char>(c));
                    out += buf;
                } else {
                    out += c;
                }
                break;
        }
    }
    return out;
}

void DevToolsHttpApi::send_raw_json(SOCKET fd, int code,
                                    const std::string& status,
                                    const std::string& json_body)
{
    std::string response =
        "HTTP/1.1 " + std::to_string(code) + " " + status + "\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: " + std::to_string(json_body.size()) + "\r\n"
        "Connection: close\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "\r\n" + json_body;
    ::send(fd, response.data(), static_cast<int>(response.size()), 0);
}

void DevToolsHttpApi::send_raw_error(SOCKET fd, int code,
                                     const std::string& message)
{
    std::string json_body = "{\"status\":\"error\",\"message\":\""
                          + escape_json(message) + "\"}";
    send_raw_json(fd, code,
                  (code == 400 ? "Bad Request" :
                   code == 404 ? "Not Found" :
                   code == 405 ? "Method Not Allowed" :
                   code == 500 ? "Internal Server Error" : "Error"),
                  json_body);
}

} // namespace devtools
} // namespace client
} // namespace chrono
