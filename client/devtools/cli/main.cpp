/**
 * main.cpp �?开发者模�?CLI 主入�?(C++23 重构�?
 *
 * REPL (Read-Eval-Print-Loop) 交互模式
 * 支持独立运行与脚本模�?
 *
 * ========== 向后兼容设计 ==========
 * 为与现有�?cmd_*.c (C 文件) 共存，此文件定义:
 *   1) C++23 命名空间 API (CommandRegistry, Config, HttpClient, TlsRaii)
 *   2) extern "C" 兼容�?(g_command_table, g_config, register_command, find_command)
 *
 * 逐步迁移: 每转换一�?cmd_*.c �?cmd_*.cpp，就移除对应�?extern "C" 依赖�?
 * ===================================
 */
#include "devtools_cli.hpp"

#include <cctype>
#include <cstdlib>
#include <format>
#include <iostream>
#include "print_compat.h"
#include <string>
#include <string_view>

#include "social/SocialManager.h"
#include <vector>

// ============================================================
// C++23 全局实例
// ============================================================
namespace cli = chrono::client::cli;

cli::CommandRegistry cli::g_command_registry;
cli::Config cli::g_cli_config;

using cli::g_command_registry;
using cli::g_cli_config;

// ============================================================
// C 兼容�?�?用于现有�?cmd_*.c (C 文件)
// 这些符号具有 C linkage，C 文件可以直接调用
// ============================================================

// 包含 C 头文件以保持类型一�?
// 注意: C 头文件定义了 typedef int (*CommandHandler)(int, char**)
// 这与 C++23 �?chrono::client::cli::CommandHandler (move_only_function) 冲突
// 因此�?extern "C" 函数中直接使�?C 函数指针类型
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
 * register_command �?C 兼容包装
 * �?C 风格命令表注册，同时同步�?C++ CommandRegistry
 * 等价�?C 头文�? void register_command(const char*, const char*, const char*, CommandHandler)
 */
extern "C" void register_command(const char* name, const char* desc,
                                  const char* usage,
                                  int (*handler)(int, char**))
{
    // C 兼容�?
    if (g_command_count >= MAX_COMMANDS) {
        cli::println(stderr, "[-] 命令注册表已满");
        return;
    }
    g_command_table[g_command_count].name        = name;
    g_command_table[g_command_count].description = desc;
    g_command_table[g_command_count].usage       = usage;
    g_command_table[g_command_count].handler     = handler;
    g_command_count++;

    // 同步�?C++ CommandRegistry
    g_command_registry.add(
        name, desc, usage,
        [handler](cli::Args args) -> int {
            // 跳过命令名 (args[0]), 与旧 main.c 的 handler(argc-1, argv+1) 一致
            auto cmd_args = args.size() > 1 ? args.subspan(1) : cli::Args{};
            std::vector<char*> argv_ptrs;
            std::vector<std::string> storage;
            for (auto& s : cmd_args) {
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

// C 兼容层: print_json (供 cmd_*.c 调用)
extern "C" void print_json(const char* json, int indent)
{
    if (json) {
        cli::print_json(std::string_view(json), indent);
    }
}

// C 兼容层: base64_decode (供 cmd_*.c 调用)
extern "C" int base64_decode(const char* in, size_t in_len,
                              unsigned char* out, size_t* out_len)
{
    auto result = cli::base64_decode(std::string_view(in, in_len));
    if (!result) return -1;
    if (result->size() > *out_len) {
        *out_len = result->size();
        return -1;
    }
    std::memcpy(out, result->data(), result->size());
    *out_len = result->size();
    return 0;
}

extern "C" void config_init_defaults(void)
{
    std::memset(&g_config, 0, sizeof(g_config));
    std::strcpy(g_config.host, "127.0.0.1");
    g_config.port    = 4443;
    g_config.use_tls = 1;
    std::strcpy(g_config.storage_path, "./data");

    // 同步�?C++ Config
    g_cli_config = cli::Config{};

    // 从环境变量读�?
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

    cli::print("{}", text);
    SetConsoleTextAttribute(hConsole, attr);
#else
    cli::print("{}{}{}", color, text, cli::COLOR_RESET);
#endif
}

void cli::print_json(std::string_view json, int indent)
{
    if (json.empty()) {
        cli::println("(�?");
        return;
    }

    int depth = 0;
    bool in_string = false;
    bool escape = false;

    for (char ch : json) {
        if (escape) {
            cli::print("{}", ch);
            escape = false;
            continue;
        }
        if (ch == '\\') {
            cli::print("{}", ch);
            escape = true;
            continue;
        }
        if (ch == '"') {
            in_string = !in_string;
            cli::print("{}", ch);
            continue;
        }
        if (!in_string) {
            if (ch == '{' || ch == '[') {
                cli::println("{}", ch);
                depth++;
                for (int i = 0; i < depth * indent; i++) cli::print(" ");
                continue;
            }
            if (ch == '}' || ch == ']') {
                cli::println("");
                depth--;
                for (int i = 0; i < depth * indent; i++) cli::print(" ");
                cli::print("{}", ch);
                continue;
            }
            if (ch == ',') {
                cli::println("{}", ch);
                for (int i = 0; i < depth * indent; i++) cli::print(" ");
                continue;
            }
            if (ch == ':') {
                cli::print("{} ", ch);
                continue;
            }
        }
        cli::print("{}", ch);
    }
    cli::println("");
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
    cli::println("");
    cli::println("  ╔══════════════════════════════════════════════════════════╗");
    cli::println("  ║     Chrono-shift 开发者模式 CLI v0.2.0 (C++23)        ║");
    cli::println("  ╚══════════════════════════════════════════════════════════╝");
    cli::println("");

    cli::println("可用命令:");
    for (const auto& cmd : g_command_registry.all()) {
        cli::println("  {:<20} {}", cmd.name, cmd.description);
    }
    cli::println("");
    cli::println("配置:");
    cli::println("  当前服务�? {}:{}", g_cli_config.host, g_cli_config.port);
    cli::println("  协议:       {}", g_cli_config.use_tls ? "HTTPS" : "HTTP");
    cli::println("  会话状态:   {}", g_cli_config.session_logged_in ? "已登录" : "未登录");
    cli::println("");
    cli::println("提示:");
    cli::println("  环境变量 CHRONO_HOST / CHRONO_PORT / CHRONO_TLS 设置目标");
    cli::println("  输入命令名查看详细用法, exit 退出");
    cli::println("");
    return 0;
}

// ============================================================
// verbose 命令 (C++23 版本)
// ============================================================
static int cmd_verbose(cli::Args args)
{
    (void)args;
    g_cli_config.verbose = !g_cli_config.verbose;
    cli::println("[*] 详细模式: {}", g_cli_config.verbose ? "开" : "关");
    // 同步�?C config
    g_config.verbose = g_cli_config.verbose ? 1 : 0;
    return 0;
}

// ============================================================
// 主入�?
// ============================================================
int main(int argc, char** argv)
{
    // 初始化配�?
    config_init_defaults();

    // 初始�?Winsock
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    // 注册内置命令 (C++23 API)
    g_command_registry.add("help", "显示此帮助信息", "help", cmd_help);
    g_command_registry.add("verbose", "切换详细模式", "verbose", cmd_verbose);

    // 同步到 C 兼容层 (help + verbose)
    register_command("help", "显示此帮助信息", "help",
                     [](int, char**) -> int { return cmd_help({}); });
    register_command("verbose", "切换详细模式", "verbose",
                     [](int, char**) -> int { return cmd_verbose({}); });

    // 引入外部命令 (�?cmd_*.c / init_commands.cpp 中注�?
    extern void init_commands(void);
    init_commands();

    // 社交模块: 加载状态, 显示待处理请求
    {
        auto& mgr = chrono::client::social::SocialManager::instance();
        mgr.load_state("./data");
        mgr.cleanup_expired_blocks();

        // 尝试自动连接 Tor (默认传输层)
        extern void try_auto_connect_tor();
        try_auto_connect_tor();

        auto pending = mgr.pending_requests();
        if (!pending.empty()) {
            cli::println("");
            cli::println("  [!] 你有 {} 个待处理的好友请求:", pending.size());
            for (auto& req : pending) {
                cli::println("      来自: {} — \"{}\" (friend accept/reject {})",
                             req.from_uid, req.greeting, req.from_uid);
            }
            cli::println("");
        }
    }

    // 脚本模式: 直接执行传入的参�?
    if (argc > 1) {
        std::vector<std::string_view> cmd_args;
        for (int i = 1; i < argc; ++i) {
            cmd_args.push_back(argv[i]);
        }
        auto* handler = g_command_registry.find(cmd_args[0]);
        if (handler) {
            return (*handler)(cmd_args);
        }
        cli::println(stderr, "未知命令: {}", cmd_args[0]);
        cmd_help({});
        return 1;
    }

    // REPL 交互模式
    cli::println("Chrono-shift 开发者模式 CLI (C++23)");
    cli::println("输入 help 查看命令, exit 退出\n");

    std::string line;
    while (true) {
        cli::print("devtools> ");
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

        // 查找并执行命�?(C++23 API)
        auto* handler = g_command_registry.find(tokens[0]);
        if (handler) {
            int ret = (*handler)(tokens);
            if (ret != 0 && g_cli_config.verbose) {
                cli::println("[-] 命令返回: {}", ret);
            }
        } else {
            cli::println("未知命令: {} (输入 help 查看可用命令)", tokens[0]);
        }
    }

#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
