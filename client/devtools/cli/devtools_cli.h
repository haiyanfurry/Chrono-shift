/**
 * devtools_cli.h — 开发者模式 CLI 公共头文件
 *
 * 定义命令注册表、全局状态、工具函数
 * 此头文件被所有 cmd_*.c 和 main.c 包含
 */
#ifndef DEVTOOLS_CLI_H
#define DEVTOOLS_CLI_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#endif

/* ============================================================
 * 命令注册表
 * ============================================================ */

/** 命令处理函数类型 */
typedef int (*CommandHandler)(int argc, char** argv);

/** 命令条目 */
typedef struct {
    const char* name;          /* 命令名称 */
    const char* description;   /* 简短描述 */
    const char* usage;         /* 用法说明 */
    CommandHandler handler;    /* 处理函数 */
} CommandEntry;

/** 最大注册命令数 */
#define MAX_COMMANDS 64

/** 命令注册表 */
extern CommandEntry g_command_table[MAX_COMMANDS];
extern int g_command_count;

/** 注册一个命令 */
void register_command(const char* name, const char* desc,
                      const char* usage, CommandHandler handler);

/** 查找命令 */
CommandHandler find_command(const char* name);

/* ============================================================
 * 全局配置
 * ============================================================ */

typedef struct {
    /* 服务器连接 */
    char host[256];
    int  port;
    int  use_tls;         /* 1=TLS, 0=明文 */

    /* 会话 */
    int  session_logged_in;
    char session_token[2048];
    char session_host[256];

    /* WebSocket */
    int  ws_connected;    /* 是否已连接 */
    void* ws_ssl;         /* SSL 句柄 (SSL*) */
    int   ws_buffer_len;  /* WS 接收缓冲区长度 */

    /* 调试 */
    int  verbose;         /* 详细模式 */
    char storage_path[512];
} DevToolsConfig;

/** 全局配置实例 */
extern DevToolsConfig g_config;

/** 初始化默认配置 */
void config_init_defaults(void);

/* ============================================================
 * 工具函数
 * ============================================================ */

/** 获取当前时间戳字符串 */
const char* timestamp_str(void);

/** 打印带颜色的文本 (Windows/Linux 兼容) */
void print_colored(const char* color, const char* format, ...);

/** JSON 格式化输出 (缩进) */
void print_json(const char* json, int indent);

/** Base64 解码 */
int base64_decode(const char* in, size_t in_len,
                  unsigned char* out, size_t* out_len);

/* 颜色常量 */
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_BOLD    "\033[1m"

#endif /* DEVTOOLS_CLI_H */
