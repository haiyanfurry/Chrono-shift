#ifndef CHRONO_SERVER_H
#define CHRONO_SERVER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ============================================================
 * Chrono-shift 服务端主接口
 * 语言标准: C99
 * ============================================================ */

/* --- 配置结构体 --- */
typedef struct {
    char host[64];            /* 监听地址 */
    uint16_t port;            /* 监听端口 */
    char db_path[512];        /* 数据库路径 */
    char storage_path[1024];  /* 文件存储路径 */
    char tls_cert[1024];      /* TLS 证书路径 */
    char tls_key[1024];       /* TLS 密钥路径 */
    char jwt_secret[128];     /* JWT 签名密钥 */
    size_t thread_pool_size;  /* 线程池大小 */
    size_t max_connections;   /* 最大连接数 */
    int log_level;            /* 日志级别 0-3 */
} ServerConfig;

/* --- 日志级别 --- */
enum LogLevel {
    LOG_DEBUG = 0,
    LOG_INFO  = 1,
    LOG_WARN  = 2,
    LOG_ERROR = 3
};

/* --- 服务器状态 --- */
typedef struct {
    bool running;
    uint64_t uptime_seconds;
    uint64_t active_connections;
    uint64_t total_requests;
    uint64_t total_messages;
} ServerStatus;

/* --- 主函数 --- */
int  server_init(const ServerConfig* config);
int  server_start(void);
void server_stop(void);
void server_get_status(ServerStatus* status);

/* --- HTTP 请求辅助函数 (在 http_server.c 中实现) --- */
/* 获取指定 header 的值，返回 NULL 如果不存在 */
const char* http_get_header_value(const char headers[64][2][1024], size_t header_count,
                                   const char* key);

/* 从 Authorization 头提取 Bearer token，返回 NULL 如果不存在 */
const char* http_extract_bearer_token(const char headers[64][2][1024], size_t header_count);

/* --- 日志工具 --- */
void log_write(int level, const char* file, int line, const char* fmt, ...);

#define LOG_DEBUG(fmt, ...) log_write(LOG_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)  log_write(LOG_INFO,  __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  log_write(LOG_WARN,  __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) log_write(LOG_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

/* ---- 安全工具函数 ---- */
char* json_escape_string(const char* input);
int   validate_input_safe(const char* input, size_t max_len);

#endif /* CHRONO_SERVER_H */
