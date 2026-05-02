/**
 * main.c — 开发者模式 CLI 主入口
 *
 * REPL (Read-Eval-Print-Loop) 交互模式
 * 支持独立运行与脚本模式
 *
 * 从 client/tools/debug_cli.c 重构而来
 */

#include "devtools_cli.h"
#include <ctype.h>

/* ============================================================
 * 全局变量定义
 * ============================================================ */

CommandEntry g_command_table[MAX_COMMANDS];
int g_command_count = 0;
DevToolsConfig g_config;

/* ============================================================
 * 命令注册
 * ============================================================ */

void register_command(const char* name, const char* desc,
                      const char* usage, CommandHandler handler)
{
    if (g_command_count >= MAX_COMMANDS) {
        fprintf(stderr, "[-] 命令注册表已满\n");
        return;
    }
    g_command_table[g_command_count].name        = name;
    g_command_table[g_command_count].description = desc;
    g_command_table[g_command_count].usage       = usage;
    g_command_table[g_command_count].handler     = handler;
    g_command_count++;
}

CommandHandler find_command(const char* name)
{
    for (int i = 0; i < g_command_count; i++) {
        if (strcmp(g_command_table[i].name, name) == 0) {
            return g_command_table[i].handler;
        }
    }
    return NULL;
}

/* ============================================================
 * 全局配置
 * ============================================================ */

void config_init_defaults(void)
{
    memset(&g_config, 0, sizeof(g_config));
    strcpy(g_config.host, "127.0.0.1");
    g_config.port    = 4443;
    g_config.use_tls = 1;
    strcpy(g_config.storage_path, "./data");

    /* 从环境变量读取覆盖 */
    char* env_host = getenv("CHRONO_HOST");
    if (env_host) {
        strncpy(g_config.host, env_host, sizeof(g_config.host) - 1);
    }
    char* env_port = getenv("CHRONO_PORT");
    if (env_port) {
        g_config.port = atoi(env_port);
    }
    char* env_tls = getenv("CHRONO_TLS");
    if (env_tls) {
        g_config.use_tls = atoi(env_tls);
    }
}

/* ============================================================
 * 工具函数
 * ============================================================ */

static char g_ts_buffer[64];

const char* timestamp_str(void)
{
    time_t now = time(NULL);
    struct tm* tm_info;
#ifdef _WIN32
    tm_info = localtime(&now);
#else
    struct tm tm_res;
    tm_info = localtime_r(&now, &tm_res);
#endif
    strftime(g_ts_buffer, sizeof(g_ts_buffer), "%H:%M:%S", tm_info);
    return g_ts_buffer;
}

void print_colored(const char* color, const char* format, ...)
{
    char buf[4096];
    va_list args;
    va_start(args, format);
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
#ifdef _WIN32
    /* Windows 控制台颜色支持 */
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hConsole, &csbi);
    WORD attr = csbi.wAttributes;

    if (strstr(color, "31"))      SetConsoleTextAttribute(hConsole, FOREGROUND_RED);
    else if (strstr(color, "32")) SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN);
    else if (strstr(color, "33")) SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN);
    else if (strstr(color, "34")) SetConsoleTextAttribute(hConsole, FOREGROUND_BLUE);
    else if (strstr(color, "36")) SetConsoleTextAttribute(hConsole, FOREGROUND_BLUE | FOREGROUND_GREEN);
    else if (strstr(color, "1"))  SetConsoleTextAttribute(hConsole, FOREGROUND_INTENSITY | attr);

    printf("%s", buf);
    SetConsoleTextAttribute(hConsole, attr);
#else
    printf("%s%s%s", color, buf, COLOR_RESET);
#endif
}

/* ============================================================
 * JSON 格式化输出 (简单实现)
 * ============================================================ */

void print_json(const char* json, int indent)
{
    if (!json || !*json) {
        printf("(空)\n");
        return;
    }

    int depth = 0;
    int in_string = 0;
    int escape = 0;

    for (const char* p = json; *p; p++) {
        if (escape) {
            putchar(*p);
            escape = 0;
            continue;
        }
        if (*p == '\\') {
            putchar(*p);
            escape = 1;
            continue;
        }
        if (*p == '"') {
            in_string = !in_string;
            putchar(*p);
            continue;
        }
        if (!in_string) {
            if (*p == '{' || *p == '[') {
                putchar(*p);
                putchar('\n');
                depth++;
                for (int i = 0; i < depth * indent; i++) putchar(' ');
                continue;
            }
            if (*p == '}' || *p == ']') {
                putchar('\n');
                depth--;
                for (int i = 0; i < depth * indent; i++) putchar(' ');
                putchar(*p);
                continue;
            }
            if (*p == ',') {
                putchar(*p);
                putchar('\n');
                for (int i = 0; i < depth * indent; i++) putchar(' ');
                continue;
            }
            if (*p == ':') {
                putchar(*p);
                putchar(' ');
                continue;
            }
        }
        putchar(*p);
    }
    putchar('\n');
}

/* ============================================================
 * Base64 解码
 * ============================================================ */

int base64_decode(const char* in, size_t in_len,
                  unsigned char* out, size_t* out_len)
{
    static const unsigned char decode_table[256] = {
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,62,0,0,0,63,52,53,54,55,56,57,58,59,60,61,0,0,0,0,0,0,
        0,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,0,0,0,0,0,
        0,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
    };

    size_t out_pos = 0;
    unsigned char buf[4];
    int buf_pos = 0;

    for (size_t i = 0; i < in_len; i++) {
        unsigned char c = (unsigned char)in[i];
        if (c == '=') break;
        unsigned char val = decode_table[c];
        if (val == 0 && c != 'A') continue; /* 跳过无效字符 */
        buf[buf_pos++] = val;
        if (buf_pos == 4) {
            out[out_pos++] = (buf[0] << 2) | (buf[1] >> 4);
            out[out_pos++] = (buf[1] << 4) | (buf[2] >> 2);
            out[out_pos++] = (buf[2] << 6) | buf[3];
            buf_pos = 0;
        }
    }

    if (buf_pos > 1) {
        out[out_pos++] = (buf[0] << 2) | (buf[1] >> 4);
    }
    if (buf_pos > 2) {
        out[out_pos++] = (buf[1] << 4) | (buf[2] >> 2);
    }

    *out_len = out_pos;
    return 0;
}

/* ============================================================
 * help 命令
 * ============================================================ */

static int cmd_help(void)
{
    printf("\n");
    printf("  ╔══════════════════════════════════════════════════════════╗\n");
    printf("  ║      Chrono-shift 开发者模式 CLI v0.1.0                 ║\n");
    printf("  ╚══════════════════════════════════════════════════════════╝\n");
    printf("\n");

    printf("可用命令:\n");
    for (int i = 0; i < g_command_count; i++) {
        printf("  %-20s %s\n", g_command_table[i].name,
               g_command_table[i].description);
    }
    printf("\n");
    printf("配置:\n");
    printf("  当前服务器: %s:%d\n", g_config.host, g_config.port);
    printf("  协议:       %s\n", g_config.use_tls ? "HTTPS" : "HTTP");
    printf("  会话状态:   %s\n", g_config.session_logged_in ? "已登录" : "未登录");
    printf("\n");
    printf("提示:\n");
    printf("  环境变量 CHRONO_HOST / CHRONO_PORT / CHRONO_TLS 设置目标\n");
    printf("  输入命令名查看详细用法, exit 退出\n");
    printf("\n");
    return 0;
}

/* ============================================================
 * verbose 命令
 * ============================================================ */

static int cmd_verbose(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    g_config.verbose = !g_config.verbose;
    printf("[*] 详细模式: %s\n", g_config.verbose ? "开" : "关");
    return 0;
}

/* ============================================================
 * 主入口
 * ============================================================ */

int main(int argc, char** argv)
{
    /* 初始化 */
    config_init_defaults();

    /* 初始化 Winsock */
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    /* 注册内置命令 */
    register_command("help",    "显示此帮助信息",           "help",      (CommandHandler)cmd_help);
    register_command("verbose", "切换详细模式",             "verbose",   cmd_verbose);

    /* 引入外部命令 (在 cmd_*.c 中通过 init_commands() 注册) */
    extern void init_commands(void);
    init_commands();

    /* 脚本模式: 直接执行传入的参数 */
    if (argc > 1) {
        /* argv[1] 是命令, argv[2..] 是参数 */
        char* cmd_argv[64];
        int cmd_argc = 0;
        for (int i = 1; i < argc && cmd_argc < 64; i++) {
            cmd_argv[cmd_argc++] = argv[i];
        }
        CommandHandler handler = find_command(cmd_argv[0]);
        if (handler) {
            return handler(cmd_argc, cmd_argv);
        }
        fprintf(stderr, "未知命令: %s\n", cmd_argv[0]);
        cmd_help();
        return 1;
    }

    /* REPL 交互模式 */
    printf("Chrono-shift 开发者模式 CLI\n");
    printf("输入 'help' 查看命令, 'exit' 退出\n\n");

    char line[4096];
    while (1) {
        printf("devtools> ");
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) {
            break;
        }

        /* 去除换行 */
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
            line[--len] = '\0';
        }

        if (len == 0) continue;

        /* exit / quit */
        if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0) {
            break;
        }

        /* 解析参数 */
        char* tokens[64];
        int token_count = 0;
        char* token = strtok(line, " \t");
        while (token && token_count < 64) {
            tokens[token_count++] = token;
            token = strtok(NULL, " \t");
        }

        if (token_count == 0) continue;

        /* 查找并执行命令 */
        CommandHandler handler = find_command(tokens[0]);
        if (handler) {
            int ret = handler(token_count, tokens);
            if (ret != 0 && g_config.verbose) {
                printf("[-] 命令返回: %d\n", ret);
            }
        } else {
            printf("未知命令: %s (输入 help 查看可用命令)\n", tokens[0]);
        }
    }

#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
