/**
 * cmd_user.cpp — 用户管理命令 (C++23 版本)
 */
#include "../devtools_cli.hpp"

#include <cstddef>
#include <print>
#include <string>
#include <string_view>

// ============================================================
// HTTP 底层函数 — 由 net_http.cpp 通过 extern "C" 提供
// ============================================================
extern "C" {
extern int http_request(const char* method, const char* path,
                        const char* body, const char* content_type,
                        char* response, size_t resp_size);
extern int http_get_status(const char* response);
extern const char* http_get_body(const char* response);
}

namespace cli = chrono::client::cli;

constexpr std::size_t BUFFER_SIZE = 65536;

/** 执行 HTTP 请求并检查状态 */
static int do_http(const char* method, const char* path,
                   const char* body, const char* content_type,
                   int& out_status, std::string_view& out_body,
                   std::string& raw)
{
    raw.assign(BUFFER_SIZE, '\0');
    if (http_request(method, path, body, content_type,
                     raw.data(), raw.size()) != 0) {
        std::println("[-] 请求失败");
        return -1;
    }
    out_status = http_get_status(raw.c_str());
    out_body = http_get_body(raw.c_str());
    return 0;
}

/** user list - 列出所有用户 */
static int cmd_user_list(void)
{
    std::println("[*] 获取用户列表...");

    std::string raw;
    int status = 0;
    std::string_view body;
    if (do_http("GET", "/api/users", nullptr, nullptr, status, body, raw) != 0)
        return -1;

    if (status >= 200 && status < 300) {
        std::println("[+] 用户列表 (HTTP {}):", status);
        if (!body.empty()) cli::print_json(body, 0);
        return 0;
    } else {
        std::println("[-] HTTP {}", status);
        if (!body.empty()) cli::print_json(body, 4);
        return -1;
    }
}

/** user get - 获取指定用户信息 */
static int cmd_user_get(const char* user_id)
{
    std::println("[*] 获取用户信息: {}", user_id);

    std::string path = std::string("/api/user/profile?id=") + user_id;

    std::string raw;
    int status = 0;
    std::string_view body;
    if (do_http("GET", path.c_str(), nullptr, nullptr, status, body, raw) != 0)
        return -1;

    if (status >= 200 && status < 300) {
        std::println("[+] 用户信息 (HTTP {}):", status);
        if (!body.empty()) cli::print_json(body, 0);
        return 0;
    } else {
        std::println("[-] HTTP {}", status);
        if (!body.empty()) cli::print_json(body, 4);
        return -1;
    }
}

/** user create - 创建用户 */
static int cmd_user_create(const char* username, const char* password,
                           const char* nickname)
{
    std::print("[*] 创建用户: username={}", username);
    if (nickname) std::print(", nickname={}", nickname);
    std::println("");

    std::string json_body;
    if (nickname) {
        json_body = std::string("{\"username\":\"") + username +
                    "\",\"password\":\"" + password +
                    "\",\"nickname\":\"" + nickname + "\"}";
    } else {
        json_body = std::string("{\"username\":\"") + username +
                    "\",\"password\":\"" + password + "\"}";
    }

    std::string raw;
    int status = 0;
    std::string_view resp_body;
    if (do_http("POST", "/api/user/register",
                json_body.c_str(), "application/json",
                status, resp_body, raw) != 0)
        return -1;

    if (status >= 200 && status < 300) {
        std::println("[+] 用户创建成功 (HTTP {}):", status);
        if (!resp_body.empty()) cli::print_json(resp_body, 0);
        return 0;
    } else {
        std::println("[-] HTTP {}", status);
        if (!resp_body.empty()) cli::print_json(resp_body, 4);
        return -1;
    }
}

/** user delete - 删除用户 */
static int cmd_user_delete(const char* user_id)
{
    std::println("[*] 删除用户: {}", user_id);

    std::string path = std::string("/api/user?id=") + user_id;

    std::string raw;
    int status = 0;
    std::string_view body;
    if (do_http("DELETE", path.c_str(), nullptr, nullptr, status, body, raw) != 0)
        return -1;

    if (status >= 200 && status < 300) {
        std::println("[+] 用户删除成功 (HTTP {})", status);
        if (!body.empty()) cli::print_json(body, 0);
        return 0;
    } else {
        std::println("[-] HTTP {}", status);
        if (!body.empty()) cli::print_json(body, 4);
        return -1;
    }
}

/** user - 用户管理入口 */
static int cmd_user(int argc, char** argv)
{
    if (argc < 1) {
        std::println("用法:");
        std::println("  user list                     - 列出所有用户");
        std::println("  user get <id>                 - 获取用户信息");
        std::println("  user create <username> <pass> [nickname] - 创建用户");
        std::println("  user delete <id>              - 删除用户");
        return -1;
    }

    const char* subcmd = argv[0];

    if (std::strcmp(subcmd, "list") == 0) {
        return cmd_user_list();
    } else if (std::strcmp(subcmd, "get") == 0) {
        if (argc < 2) {
            std::println(stderr, "用法: user get <user_id>");
            return -1;
        }
        return cmd_user_get(argv[1]);
    } else if (std::strcmp(subcmd, "create") == 0) {
        if (argc < 3) {
            std::println(stderr, "用法: user create <username> <password> [nickname]");
            return -1;
        }
        return cmd_user_create(argv[1], argv[2], argc >= 4 ? argv[3] : nullptr);
    } else if (std::strcmp(subcmd, "delete") == 0) {
        if (argc < 2) {
            std::println(stderr, "用法: user delete <user_id>");
            return -1;
        }
        return cmd_user_delete(argv[1]);
    } else {
        std::println(stderr, "未知 user 子命令: {}", subcmd);
        std::println(stderr, "可用命令: list, get, create, delete");
        return -1;
    }
}

extern "C" void init_cmd_user(void)
{
    register_command("user",
                     "用户管理 (list/get/create/delete)",
                     "user <list|get|create|delete> ...",
                     cmd_user);
}
