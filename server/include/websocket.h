#ifndef CHRONO_WEBSOCKET_H
#define CHRONO_WEBSOCKET_H

#include "server.h"
#include "http_server.h"
#include "platform_compat.h"
#include <stdint.h>
#include <stdbool.h>

/* ============================================================
 * WebSocket (RFC 6455) 实现
 * ============================================================ */

/* --- WebSocket 帧类型 --- */
enum WsOpcode {
    WS_OPCODE_CONTINUATION = 0x0,
    WS_OPCODE_TEXT         = 0x1,
    WS_OPCODE_BINARY       = 0x2,
    WS_OPCODE_CLOSE        = 0x8,
    WS_OPCODE_PING         = 0x9,
    WS_OPCODE_PONG         = 0xA
};

/* --- WebSocket 连接 (前向声明) --- */
typedef struct WsConnection WsConnection;

/* --- 消息回调 --- */
typedef void (*WsMessageCallback)(WsConnection* conn, enum WsOpcode opcode,
                                  const uint8_t* data, size_t length);
typedef void (*WsCloseCallback)(WsConnection* conn, uint16_t code, const char* reason);
typedef void (*WsConnectCallback)(WsConnection* conn);

/* --- API --- */
int  ws_init(void);
int  ws_handle_upgrade(const HttpRequest* req, HttpResponse* resp,
                       WsMessageCallback on_msg, WsCloseCallback on_close,
                       WsConnectCallback on_connect, void* user_data);
int  ws_send(WsConnection* conn, enum WsOpcode opcode, const uint8_t* data, size_t length);
int  ws_send_text(WsConnection* conn, const char* text);
int  ws_send_binary(WsConnection* conn, const uint8_t* data, size_t length);
int  ws_close(WsConnection* conn, uint16_t code, const char* reason);
void ws_get_peer_addr(WsConnection* conn, char* addr_buf, size_t buf_size);

/* 内部: 从已升级的 socket 创建 WS 连接 */
WsConnection* ws_create_connection(socket_t fd, const struct sockaddr_in* addr,
                                    WsMessageCallback on_msg, WsCloseCallback on_close,
                                    WsConnectCallback on_connect, void* user_data);

#endif /* CHRONO_WEBSOCKET_H */
