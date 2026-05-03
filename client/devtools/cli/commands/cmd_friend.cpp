/**
 * cmd_friend.cpp - 好友操作命令
 * 对应 debug_cli.c:2097 cmd_friend
 *
 * C++23 转换: std::println, std::string, std::string_view
 */
#include "../devtools_cli.hpp"

#include "print_compat.h     // std::println
#include <string>    // std::string
#include <string_view> // std::string_view

namespace cli = chrono::client::cli;

/* ============================================================
 * HTTP 请求 (�?net_http.cpp 提供 extern "C" 兼容�?
 * ============================================================ */
extern "C" {
extern int http_request(const char* method, const char* path,
                        const char* body, const char* content_type,
                        char* response, size_t resp_size);
extern int http_get_status(const char* response);
extern const char* http_get_body(const char* response);
}

constexpr size_t BUFFER_SIZE = 65536;

/** friend - 好友操作命令 */
static int cmd_friend(int argc, char** argv)
{
    if (argc < 1) {
        cli::println("用法:");
        cli::println("  friend list <user_id>         - 列出用户好友列表");
        cli::println("  friend add <uid1> <uid2>      - 添加好友关系");
        return -1;
    }

    const std::string_view subcmd = argv[0];
    char response[BUFFER_SIZE]{};

    if (subcmd == "list") {
        if (argc < 2) { cli::println(stderr, "用法: friend list <user_id>"); return -1; }
        const char* uid = argv[1];
        std::string path = std::string("/api/friends?id=") + uid;
        cli::println("[*] 获取好友列表: user_id={}", uid);

        if (http_request("GET", path.c_str(), nullptr, nullptr, response, sizeof(response)) != 0) {
            cli::println("[-] 请求失败"); return -1;
        }
        int status = http_get_status(response);
        const char* body = http_get_body(response);
        if (status >= 200 && status < 300) {
            cli::println("[+] 好友列表 (HTTP {}):", status);
            if (std::strlen(body) > 0) cli::print_json(body, 0);
            return 0;
        } else {
            cli::println("[-] HTTP {}", status);
            if (std::strlen(body) > 0) cli::print_json(body, 4);
            return -1;
        }

    } else if (subcmd == "add") {
        if (argc < 3) { cli::println(stderr, "用法: friend add <user_id1> <user_id2>"); return -1; }
        std::string body = std::string("{\"user_id1\":")
            + argv[1] + ",\"user_id2\":" + argv[2] + "}";
        cli::println("[*] 添加好友: {} <-> {}", argv[1], argv[2]);

        if (http_request("POST", "/api/friends/add", body.c_str(), "application/json",
                          response, sizeof(response)) != 0) {
            cli::println("[-] 请求失败"); return -1;
        }
        int status = http_get_status(response);
        const char* resp_body = http_get_body(response);
        if (status >= 200 && status < 300) {
            cli::println("[+] 好友添加成功 (HTTP {}):", status);
            if (std::strlen(resp_body) > 0) cli::print_json(resp_body, 0);
            return 0;
        } else {
            cli::println("[-] HTTP {}", status);
            if (std::strlen(resp_body) > 0) cli::print_json(resp_body, 4);
            return -1;
        }

    } else {
        cli::println(stderr, "未知 friend 子命�? {}", subcmd);
        return -1;
    }
}

extern "C" void init_cmd_friend(void)
{
    register_command("friend",
                     "好友操作 (list/add)",
                     "friend <list|add> ...",
                     cmd_friend);
}
