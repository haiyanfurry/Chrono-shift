/**
 * cmd_db.cpp - 数据库调试命令
 * 对应 debug_cli.c:2180 cmd_db
 *
 * C++23 转换: std::println, std::string, std::string_view
 */
#include "../devtools_cli.hpp"

#include "print_compat.h"     // std::println
#include <string>    // std::string
#include <string_view> // std::string_view
#include <cstring>   // std::strlen

namespace cli = chrono::client::cli;

/* ============================================================
 * HTTP 请求 (由 net_http.cpp 提供 extern "C" 兼容接口)
 * ============================================================ */
extern "C" {
extern int http_request(const char* method, const char* path,
                        const char* body, const char* content_type,
                        char* response, size_t resp_size);
extern int http_get_status(const char* response);
extern const char* http_get_body(const char* response);
}

constexpr size_t BUFFER_SIZE = 65536;

/* cmd_user_list 由 cmd_user.cpp 实现 */
extern "C" int cmd_user_list(void);

/** db - 数据库调试命令 */
static int cmd_db(int argc, char** argv)
{
    if (argc < 1) {
        cli::println("用法:");
        cli::println("  db list users                  - 列出所有用户");
        cli::println("  db list messages               - 列出消息数据");
        cli::println("  db list friends                - 列出好友关系");
        cli::println("  db list templates              - 列出所有模板");
        return -1;
    }

    const std::string_view subcmd = argv[0];

    if (subcmd == "list") {
        if (argc < 2) {
            cli::println(stderr, "用法: db list <type> (users/messages/friends/templates)");
            return -1;
        }
        const std::string_view type = argv[1];
        std::string path;

        if (type == "users") {
            return cmd_user_list();
        } else if (type == "messages") {
            path = "/api/messages";
        } else if (type == "friends") {
            path = "/api/friendships";
        } else if (type == "templates") {
            path = "/api/templates?limit=100&offset=0";
        } else {
            cli::println(stderr, "未知类型: {} (可用: users, messages, friends, templates)", type);
            return -1;
        }

        cli::println("[*] 查询数据库: {}", path);
        char response[BUFFER_SIZE]{};
        if (http_request("GET", path.c_str(), nullptr, nullptr, response, sizeof(response)) != 0) {
            cli::println("[-] 请求失败"); return -1;
        }
        int status = http_get_status(response);
        const char* body = http_get_body(response);
        if (status >= 200 && status < 300) {
            cli::println("[+] 数据 (HTTP {}):", status);
            if (std::strlen(body) > 0) cli::print_json(body, 0);
            return 0;
        } else {
            cli::println("[-] HTTP {}", status);
            if (std::strlen(body) > 0) cli::print_json(body, 4);
            return -1;
        }
    } else {
        cli::println(stderr, "未知 db 子命令: {}", subcmd);
        return -1;
    }
}

extern "C" void init_cmd_db(void)
{
    register_command("db",
                     "数据库调试 (list users/messages/friends/templates)",
                     "db list <type>",
                     cmd_db);
}
