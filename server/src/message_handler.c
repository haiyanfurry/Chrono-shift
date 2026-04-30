/**
 * Chrono-shift 消息处理 HTTP/WS 处理器 (骨架)
 * 语言标准: C99
 */

#include "message_handler.h"
#include "database.h"
#include "websocket.h"
#include "json_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void handle_send_message(const HttpRequest* req, HttpResponse* resp, void* user_data)
{
    (void)req;
    (void)user_data;
    http_response_init(resp);
    http_response_set_json(resp, json_build_error("功能开发中 - Phase 4"));
}

void handle_get_messages(const HttpRequest* req, HttpResponse* resp, void* user_data)
{
    (void)req;
    (void)user_data;
    http_response_init(resp);
    http_response_set_json(resp, json_build_error("功能开发中 - Phase 4"));
}

void ws_on_message(WsConnection* conn, enum WsOpcode opcode,
                   const uint8_t* data, size_t length)
{
    (void)conn;
    (void)opcode;
    (void)data;
    (void)length;
    /* Phase 4 实现 WebSocket 消息处理 */
}

void ws_on_close(WsConnection* conn, uint16_t code, const char* reason)
{
    (void)conn;
    (void)code;
    (void)reason;
    /* Phase 4 实现关闭处理 */
}

void ws_on_connect(WsConnection* conn)
{
    (void)conn;
    /* Phase 4 实现连接处理 */
}
