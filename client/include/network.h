#ifndef CHRONO_CLIENT_NETWORK_H
#define CHRONO_CLIENT_NETWORK_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ============================================================
 * 客户端网络通信层
 * HTTP/WebSocket 客户端
 * ============================================================ */

/* 重连配置 */
#define MAX_RECONNECT_DELAY_MS      30000   /* 最大重连间隔 30 秒 */
#define INITIAL_RECONNECT_DELAY_MS  500     /* 初始重连间隔 500 毫秒 */
#define RECONNECT_BACKOFF_FACTOR    2       /* 指数退避因子 */

typedef struct {
    int sock;                       /* socket 描述符 (HTTP 模式) */
    void* ssl;                      /* SSL 对象指针 (HTTPS 模式) */
    char server_host[256];          /* 服务器地址 */
    uint16_t server_port;           /* 服务器端口 */
    bool connected;                 /* 是否已连接 */
    bool use_tls;                   /* 是否使用 TLS */
    char auth_token[512];           /* 认证令牌 */
    /* 自动重连状态 */
    int  reconnect_count;           /* 连续重连次数 */
    int  max_retries;               /* 最大重试次数 (-1=无限) */
    bool auto_reconnect;            /* 启用自动重连 */
} NetworkContext;

/* --- API --- */
int  net_init(NetworkContext* ctx);
void net_cleanup(void);
int  net_connect(NetworkContext* ctx, const char* host, uint16_t port);
int  net_set_tls(NetworkContext* ctx, bool enable);
int  net_reconnect(NetworkContext* ctx);
int  net_http_request(NetworkContext* ctx, const char* method, const char* path,
                      const char* headers, const uint8_t* body, size_t body_len,
                      char** response_body, size_t* response_len);
int  net_ws_connect(NetworkContext* ctx, const char* path);
int  net_ws_send(NetworkContext* ctx, const uint8_t* data, size_t length);
int  net_ws_recv(NetworkContext* ctx, uint8_t* buffer, size_t buf_size, size_t* received);
void net_disconnect(NetworkContext* ctx);
bool net_is_connected(NetworkContext* ctx);
int  net_set_auto_reconnect(NetworkContext* ctx, bool enable, int max_retries);

#endif /* CHRONO_CLIENT_NETWORK_H */
