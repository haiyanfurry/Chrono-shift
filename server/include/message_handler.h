#ifndef CHRONO_MESSAGE_HANDLER_H
#define CHRONO_MESSAGE_HANDLER_H

#include "http_server.h"
#include "websocket.h"

/* ============================================================
 * 消息处理 HTTP/WS 处理器
 * ============================================================ */

/* HTTP API */
void handle_send_message(const HttpRequest* req, HttpResponse* resp, void* user_data);
void handle_get_messages(const HttpRequest* req, HttpResponse* resp, void* user_data);

/* WebSocket 消息处理器 */
void ws_on_message(WsConnection* conn, enum WsOpcode opcode, 
                   const uint8_t* data, size_t length);
void ws_on_close(WsConnection* conn, uint16_t code, const char* reason);
void ws_on_connect(WsConnection* conn);

#endif /* CHRONO_MESSAGE_HANDLER_H */
