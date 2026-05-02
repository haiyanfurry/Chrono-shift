#ifndef CHRONO_CLIENT_H
#define CHRONO_CLIENT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ============================================================
 * Chrono-shift 客户端主接口
 * 语言标准: C99
 * ============================================================ */

/* --- 客户端配置 --- */
typedef struct {
    char server_host[256];     /* 服务器地址 */
    uint16_t server_port;      /* 服务器端口 */
    char app_data_path[1024];  /* 本地数据路径 */
    int log_level;             /* 日志级别 */
    bool auto_reconnect;       /* 自动重连 */
} ClientConfig;

/* --- 客户端状态 --- */
typedef struct {
    bool connected;
    bool logged_in;
    char user_id[64];
    char username[128];
    char current_token[512];
    uint64_t last_heartbeat;
} ClientState;

/* --- 日志级别 --- */
enum LogLevel {
    LOG_DEBUG = 0,
    LOG_INFO  = 1,
    LOG_WARN  = 2,
    LOG_ERROR = 3
};

/* --- 主函数 --- */
int  client_init(const ClientConfig* config);
int  client_start(void);
void client_stop(void);
void client_get_state(ClientState* state);

/* --- 日志工具 --- */
void log_write(int level, const char* file, int line, const char* fmt, ...);

#define LOG_DEBUG(fmt, ...) log_write(LOG_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)  log_write(LOG_INFO,  __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  log_write(LOG_WARN,  __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) log_write(LOG_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#endif /* CHRONO_CLIENT_H */
