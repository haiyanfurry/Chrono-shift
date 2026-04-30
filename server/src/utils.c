/**
 * Chrono-shift 工具函数
 * 语言标准: C99
 */

#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

/* 当前日志级别 */
static int g_log_level = LOG_INFO;

/* 日志级别名称 */
static const char* log_level_names[] = {
    "DEBUG", "INFO", "WARN", "ERROR"
};

void log_write(int level, const char* file, int line, const char* fmt, ...)
{
    if (level < g_log_level) {
        return;
    }

    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char time_buf[32];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);

    /* 获取文件名（不含路径） */
    const char* filename = file;
    const char* slash = strrchr(file, '/');
    if (slash) filename = slash + 1;
    slash = strrchr(file, '\\');
    if (slash) filename = slash + 1;

    /* 格式化时间前缀 */
    printf("[%s] [%s] [%s:%d] ", 
           time_buf, log_level_names[level], filename, line);

    /* 格式化用户消息 */
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);

    printf("\n");
    fflush(stdout);
}

void log_set_level(int level)
{
    if (level >= LOG_DEBUG && level <= LOG_ERROR) {
        g_log_level = level;
    }
}

/* 获取当前时间的毫秒数 */
uint64_t get_timestamp_ms(void)
{
    time_t s = time(NULL);
    return (uint64_t)s * 1000;
}

/* 生成 UUID v4（简单实现） */
void generate_uuid(char* buf, size_t buf_size)
{
    if (buf_size < 37) return;
    
    const char* hex = "0123456789abcdef";
    for (int i = 0; i < 36; i++) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            buf[i] = '-';
        } else if (i == 14) {
            buf[i] = '4';
        } else {
            buf[i] = hex[rand() % 16];
        }
    }
    buf[36] = '\0';
}
