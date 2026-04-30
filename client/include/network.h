#ifndef CHRONO_CLIENT_NETWORK_H
#define CHRONO_CLIENT_NETWORK_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ============================================================
 * 客户端网络通信层
 * HTTP/WebSocket 客户端
 * ============================================================ */

typedef struct {
    int sock;                       /* socket 描述符 */
    char server_host[256];          /* 服务器地址 */
    uint16_t server_port;           /* 服务器端口 */
    bool connected;                 /* 是否已连接 */
    char auth_token[512];           /* 认证令牌 */
} NetworkContext;

/* --- API --- */
int  net_init(NetworkContext* ctx);
void net_cleanup(void);
int  net_connect(NetworkContext* ctx, const char* host, uint16_t port);
int  net_http_request(NetworkContext* ctx, const char* method, const char* path,
                      const char* headers, const uint8_t* body, size_t body_len,
                      char** response_body, size_t* response_len);
int  net_ws_connect(NetworkContext* ctx, const char* path);
int  net_ws_send(NetworkContext* ctx, const uint8_t* data, size_t length);
int  net_ws_recv(NetworkContext* ctx, uint8_t* buffer, size_t buf_size, size_t* received);
void net_disconnect(NetworkContext* ctx);
bool net_is_connected(NetworkContext* ctx);

#endif /* CHRONO_CLIENT_NETWORK_H */
