/**
 * main.cpp — 开发者模式 CLI 主入口 (C++23 重构版)
 *
 * REPL (Read-Eval-Print-Loop) 交互模式
 * 支持独立运行与脚本模式
 *
 * ========== 向后兼容设计 ==========
 * 为与现有的 cmd_*.c (C 文件) 共存，此文件定义:
 *   1) C++23 命名空间 API (CommandRegistry, Config, HttpClient, TlsRaii)
 *   2) extern "C" 兼容层 (g_command_table, g_config, register_command, find_command)
 *
 * 逐步迁移: 每转换一个 cmd_*.c → cmd_*.cpp，就移除对应的 extern "C" 依赖。
 * ===================================
 */
#include "devtools_cli.hpp"

#include <cctype>
#include <cstdlib>
#include <format>
#include <iostream>
#include <print>
#include <string>
#include <string_view>
#include <vector>

// ============================================================
// C++23 全局实例
// ============================================================
namespace cli = chrono::client::cli;
using namespace cli;

cli::CommandRegistry cli::g_command_registry;
cli::Config cli::g_cli_config;

// ============================================================
// C 兼容层 — 用于现有的 cmd_*.c (C 文件)
// 这些符号具有 C linkage，C 文件可以直接调用
// ============================================================

// 包含 C 头文件以保持类型一致
// 注意: C 头文件定义了 typedef int (*CommandHandler)(int, char**)
// 这与 C++23 的 chrono::client::cli::CommandHandler (move_only_function) 冲突
// 因此在 extern "C" 函数中直接使用 C 函数指针类型
extern "C" {
    #include "devtools_cli.h"
}

// C 全局变量定义
extern "C" {
    CommandEntry g_command_table[MAX_COMMANDS];
    int g_command_count = 0;
    DevToolsConfig g_config;
}

/**
 * register_command — C 兼容包装
 * 向 C 风格命令表注册，同时同步到 C++ CommandRegistry
 * 等价于 C 头文件: void register_command(const char*, const char*, const char*, CommandHandler)
 */
extern "C" void register_command(const char* name, const char* desc,
                                  const char* usage,
                                  int (*handler)(int, char**))
{
    // C 兼容表
    if (g_command_count >= MAX_COMMANDS) {
        std::println(stderr, "[-] 命令注册表已满");
        return;
    }
    g_command_table[g_command_count].name        = name;
    g_command_table[g_command_count].description = desc;
    g_command_table[g_command_count].usage       = usage;
    g_command_table[g_command_count].handler     = handler;
    g_command_count++;

    // 同步到 C++ CommandRegistry
    g_command_registry.add(
        name, desc, usage,
        [handler](cli::Args args) -> int {
            // 将 C++ span<string_view> 转换为 C argc/argv
            std::vector<char*> argv_ptrs;
            std::vector<std::string> storage;
            for (auto& s : args) {
                storage.push_back(std::string(s));
            }
            for (auto& s : storage) {
                argv_ptrs.push_back(s.data());
            }
            return handler(static_cast<int>(argv_ptrs.size()),
                           argv_ptrs.data());
        });
}

extern "C" int (*find_command(const char* name))(int, char**)
{
    for (int i = 0; i < g_command_count; i++) {
        if (std::strcmp(g_command_table[i].name, name) == 0) {
            return g_command_table[i].handler;
        }
    }
    return nullptr;
}

extern "C" void config_init_defaults(void)
{
    std::memset(&g_config, 0, sizeof(g_config));
    std::strcpy(g_config.host, "127.0.0.1");
    g_config.port    = 4443;
    g_config.use_tls = 1;
    std::strcpy(g_config.storage_path, "./data");

    // 同步到 C++ Config
    g_cli_config = cli::Config{};

    // 从环境变量读取
    char* env_host = std::getenv("CHRONO_HOST");
    if (env_host) {
        std::strncpy(g_config.host, env_host, sizeof(g_config.host) - 1);
        g_cli_config.host = env_host;
    }
    char* env_port = std::getenv("CHRONO_PORT");
    if (env_port) {
        g_config.port = std::atoi(env_port);
        g_cli_config.port = static_cast<uint16_t>(std::atoi(env_port));
    }
    char* env_tls = std::getenv("CHRONO_TLS");
    if (env_tls) {
        g_config.use_tls = std::atoi(env_tls);
        g_cli_config.use_tls = (std::atoi(env_tls) != 0);
    }
}

// ============================================================
// C++ 工具函数实现
// ============================================================

std::string cli::timestamp_str()
{
    auto now = std::chrono::system_clock::now();
    auto tt = std::chrono::system_clock::to_time_t(now);
    auto tm = *std::localtime(&tt);
    return std::format("{:02}:{:02}:{:02}", tm.tm_hour, tm.tm_min, tm.tm_sec);
}

void cli::print_colored(std::string_view color, std::string_view text)
{
#ifdef _WIN32
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hConsole, &csbi);
    WORD attr = csbi.wAttributes;

    if (color.find("31") != std::string_view::npos)
        SetConsoleTextAttribute(hConsole, FOREGROUND_RED);
    else if (color.find("32") != std::string_view::npos)
        SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN);
    else if (color.find("33") != std::string_view::npos)
        SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN);
    else if (color.find("34") != std::string_view::npos)
        SetConsoleTextAttribute(hConsole, FOREGROUND_BLUE);
    else if (color.find("36") != std::string_view::npos)
        SetConsoleTextAttribute(hConsole, FOREGROUND_BLUE | FOREGROUND_GREEN);
    else if (color.find("1") != std::string_view::npos)
        SetConsoleTextAttribute(hConsole, FOREGROUND_INTENSITY | attr);

    std::print("{}", text);
    SetConsoleTextAttribute(hConsole, attr);
#else
    std::print("{}{}{}", color, text, cli::COLOR_RESET);
#endif
}

void cli::print_json(std::string_view json, int indent)
{
    if (json.empty()) {
        std::println("(空)");
        return;
    }

    int depth = 0;
    bool in_string = false;
    bool escape = false;

    for (char ch : json) {
        if (escape) {
            std::print("{}", ch);
            escape = false;
            continue;
        }
        if (ch == '\\') {
            std::print("{}", ch);
            escape = true;
            continue;
        }
        if (ch == '"') {
            in_string = !in_string;
            std::print("{}", ch);
            continue;
        }
        if (!in_string) {
            if (ch == '{' || ch == '[') {
                std::println("{}", ch);
                depth++;
                for (int i = 0; i < depth * indent; i++) std::print(" ");
                continue;
            }
            if (ch == '}' || ch == ']') {
                std::println("");
                depth--;
                for (int i = 0; i < depth * indent; i++) std::print(" ");
                std::print("{}", ch);
                continue;
            }
            if (ch == ',') {
                std::println("{}", ch);
                for (int i = 0; i < depth * indent; i++) std::print(" ");
                continue;
            }
            if (ch == ':') {
                std::print("{} ", ch);
                continue;
            }
        }
        std::print("{}", ch);
    }
    std::println("");
}

auto cli::base64_decode(std::string_view in)
    -> std::expected<std::vector<std::uint8_t>, std::string>
{
    static constexpr std::uint8_t DECODE_TABLE[256] = {
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,62,0,0,0,63,52,53,54,55,56,57,58,59,60,61,0,0,0,0,0,0,
        0,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,0,0,0,0,0,
        0,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
    };

    std::vector<std::uint8_t> out;
    out.reserve(in.size() * 3 / 4 + 4);

    std::uint8_t buf[4];
    int buf_pos = 0;

    for (auto c : in) {
        auto uc = static_cast<std::uint8_t>(c);
        if (uc == '=') break;
        auto val = DECODE_TABLE[uc];
        if (val == 0 && uc != 'A') continue;
        buf[buf_pos++] = val;
        if (buf_pos == 4) {
            out.push_back(static_cast<std::uint8_t>((buf[0] << 2) | (buf[1] >> 4)));
            out.push_back(static_cast<std::uint8_t>((buf[1] << 4) | (buf[2] >> 2)));
            out.push_back(static_cast<std::uint8_t>((buf[2] << 6) | buf[3]));
            buf_pos = 0;
        }
    }

    if (buf_pos > 1) {
        out.push_back(static_cast<std::uint8_t>((buf[0] << 2) | (buf[1] >> 4)));
    }
    if (buf_pos > 2) {
        out.push_back(static_cast<std::uint8_t>((buf[1] << 4) | (buf[2] >> 2)));
    }

    return out;
}

// ============================================================
// help 命令 (C++23 版本)
// ============================================================
static int cmd_help(cli::Args args)
{
    (void)args;
    std::println("");
    std::println("  ╔══════════════════════════════════════════════════════════╗");
    std::println("  ║      Chrono-shift 开发者模式 CLI v0.2.0 (C++23)        ║");
    std::println("  ╚══════════════════════════════════════════════════════════╝");
    std::println("");

    std::println("可用命令:");
    for (const auto& cmd : g_command_registry.all()) {
        std::println("  {:<20} {}", cmd.name, cmd.description);
    }
    std::println("");
    std::println("配置:");
    std::println("  当前服务器: {}:{}", g_cli_config.host, g_cli_config.port);
    std::println("  协议:       {}", g_cli_config.use_tls ? "HTTPS" : "HTTP");
    std::println("  会话状态:   {}", g_cli_config.session_logged_in ? "已登录" : "未登录");
    std::println("");
    std::println("提示:");
    std::println("  环境变量 CHRONO_HOST / CHRONO_PORT / CHRONO_TLS 设置目标");
    std::println("  输入命令名查看详细用法, exit 退出");
    std::println("");
    return 0;
}

// ============================================================
// verbose 命令 (C++23 版本)
// ============================================================
static int cmd_verbose(cli::Args args)
{
    (void)args;
    g_cli_config.verbose = !g_cli_config.verbose;
    std::println("[*] 详细模式: {}", g_cli_config.verbose ? "开" : "关");
    // 同步到 C config
    g_config.verbose = g_cli_config.verbose ? 1 : 0;
    return 0;
}

// ============================================================
// 主入口
// ============================================================
int main(int argc, char** argv)
{
    // 初始化配置
    config_init_defaults();

    // 初始化 Winsock
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    // 注册内置命令 (C++23 API)
    g_command_registry.add("help", "显示此帮助信息", "help", cmd_help);
    g_command_registry.add("verbose", "切换详细模式", "verbose", cmd_verbose);

    // 同步到 C 兼容表 (help + verbose)
    register_command("help", "显示此帮助信息", "help",
                     [](int, char**) -> int { return cmd_help({}); });
    register_command("verbose", "切换详细模式", "verbose",
                     [](int, char**) -> int { return cmd_verbose({}); });

    // 引入外部命令 (在 cmd_*.c / init_commands.cpp 中注册)
    extern void init_commands(void);
    init_commands();

    // 脚本模式: 直接执行传入的参数
    if (argc > 1) {
        std::vector<std::string_view> cmd_args;
        for (int i = 1; i < argc; ++i) {
            cmd_args.push_back(argv[i]);
        }
        auto* handler = g_command_registry.find(cmd_args[0]);
        if (handler) {
            return (*handler)(cmd_args);
        }
        std::println(stderr, "未知命令: {}", cmd_args[0]);
        cmd_help({});
        return 1;
    }

    // REPL 交互模式
    std::println("Chrono-shift 开发者模式 CLI (C++23)");
    std::println("输入 'help' 查看命令, 'exit' 退出\n");

    std::string line;
    while (true) {
        std::print("devtools> ");
        std::fflush(stdout);

        if (!std::getline(std::cin, line)) {
            break;
        }

        // 去除首尾空白
        auto trim_start = line.find_first_not_of(" \t\r\n");
        if (trim_start == std::string::npos) continue;
        auto trim_end = line.find_last_not_of(" \t\r\n");
        line = line.substr(trim_start, trim_end - trim_start + 1);

        if (line.empty()) continue;

        // exit / quit
        if (line == "exit" || line == "quit") {
            break;
        }

        // 解析参数
        std::vector<std::string_view> tokens;
        size_t pos = 0;
        while (pos < line.size()) {
            // 跳过空白
            while (pos < line.size() && std::isspace(
                static_cast<unsigned char>(line[pos]))) {
                ++pos;
            }
            if (pos >= line.size()) break;

            // 找到 token 结束位置
            size_t start = pos;
            while (pos < line.size() && !std::isspace(
                static_cast<unsigned char>(line[pos]))) {
                ++pos;
            }
            tokens.push_back(
                std::string_view(line.data() + start, pos - start));
        }

        if (tokens.empty()) continue;

        // 查找并执行命令 (C++23 API)
        auto* handler = g_command_registry.find(tokens[0]);
        if (handler) {
            int ret = (*handler)(tokens);
            if (ret != 0 && g_cli_config.verbose) {
                std::println("[-] 命令返回: {}", ret);
            }
        } else {
            std::println("未知命令: {} (输入 help 查看可用命令)", tokens[0]);
        }
    }

#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
