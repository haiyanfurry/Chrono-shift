/**
 * cmd_msg.cpp - 消息操作命令
 * 对应 debug_cli.c:1998 cmd_msg
 *
 * C++23 转换: std::println, std::string, std::string_view
 */
#include "../devtools_cli.hpp"

#include <print>     // std::println
#include <string>    // std::string
#include <string_view> // std::string_view

namespace cli = chrono::client::cli;

/* ============================================================
 * HTTP 请求 (由 net_http.cpp 提供 extern "C" 兼容层)
 * ============================================================ */
extern "C" {
extern int http_request(const char* method, const char* path,
                        const char* body, const char* content_type,
                        char* response, size_t resp_size);
extern int http_get_status(const char* response);
extern const char* http_get_body(const char* response);
}

constexpr size_t BUFFER_SIZE = 65536;

/** msg - 消息操作命令 */
static int cmd_msg(int argc, char** argv)
{
    if (argc < 1) {
        std::println("用法:");
        std::println("  msg list <user_id> [limit] [offset]  - 列出用户消息");
        std::println("  msg get <msg_id>                     - 获取消息详情");
        std::println("  msg send <to_user_id> <text>         - 发送测试消息");
        return -1;
    }

    const std::string_view subcmd = argv[0];
    char response[BUFFER_SIZE]{};

    if (subcmd == "list") {
        if (argc < 2) {
            std::println(stderr, "用法: msg list <user_id> [limit] [offset]");
            return -1;
        }
        const char* uid = argv[1];
        const char* limit = (argc >= 3) ? argv[2] : "50";
        const char* offset = (argc >= 4) ? argv[3] : "0";

        std::string path = std::string("/api/message/list?user_id=")
            + uid + "&limit=" + limit + "&offset=" + offset;
        std::println("[*] 获取消息列表: user_id={}, limit={}, offset={}", uid, limit, offset);

        if (http_request("GET", path.c_str(), nullptr, nullptr, response, sizeof(response)) != 0) {
            std::println("[-] 请求失败"); return -1;
        }
        int status = http_get_status(response);
        const char* body = http_get_body(response);
        if (status >= 200 && status < 300) {
            std::println("[+] 消息列表 (HTTP {}):", status);
            if (std::strlen(body) > 0) print_json(body, 0);
            return 0;
        } else {
            std::println("[-] HTTP {}", status);
            if (std::strlen(body) > 0) print_json(body, 4);
            return -1;
        }

    } else if (subcmd == "get") {
        if (argc < 2) { std::println(stderr, "用法: msg get <msg_id>"); return -1; }
        std::println("[-] msg get 需要服务器实现 GET /api/message/get?id=X 端点");
        return -1;

    } else if (subcmd == "send") {
        if (argc < 3) { std::println(stderr, "用法: msg send <to_user_id> <text>"); return -1; }
        const char* to_uid = argv[1];
        const char* text = argv[2];

        std::string body = std::string("{\"to_user_id\":")
            + to_uid + ",\"content\":\"" + text + "\"}";
        std::println("[*] 发送消息: to={}, text={}", to_uid, text);

        if (http_request("POST", "/api/message/send",
                          body.c_str(), "application/json",
                          response, sizeof(response)) != 0) {
            std::println("[-] 请求失败"); return -1;
        }
        int status = http_get_status(response);
        const char* resp_body = http_get_body(response);
        if (status >= 200 && status < 300) {
            std::println("[+] 消息发送成功 (HTTP {}):", status);
            if (std::strlen(resp_body) > 0) print_json(resp_body, 0);
            return 0;
        } else {
            std::println("[-] HTTP {}", status);
            if (std::strlen(resp_body) > 0) print_json(resp_body, 4);
            return -1;
        }

    } else {
        std::println(stderr, "未知 msg 子命令: {}", subcmd);
        return -1;
    }
}

extern "C" void init_cmd_msg(void)
{
    register_command("msg",
                     "消息操作 (list/get/send)",
                     "msg <list|get|send> ...",
                     cmd_msg);
}
