/**
 * Chrono-shift WebSocket 实现 (RFC 6455)
 * 语言标准: C99
 *
 * 完整的 WebSocket 握手、帧编码/解码、掩码处理
 * 跨平台: 使用 platform_compat.h 抽象层
 */

#include "platform_compat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "websocket.h"
#include "server.h"

/* ============================================================
 * 常量
 * ============================================================ */
#define WS_GUID          "258EAFA5-E914-47DA-95CA-5AB9B49B8A85"
#define WS_MAX_FRAME_SIZE (64 * 1024)
#define MAX_WS_CONNECTIONS 1024

/* ============================================================
 * WebSocket 连接结构
 * ============================================================ */
struct WsConnection {
    socket_t fd;
    char peer_addr[64];
    uint16_t peer_port;
    WsMessageCallback on_message;
    WsCloseCallback on_close;
    WsConnectCallback on_connect;
    void* user_data;
    bool closed;
    bool client_mask;       /* 客户端发送需要掩码 */
    uint8_t recv_buffer[WS_MAX_FRAME_SIZE];
    size_t recv_offset;
};

/* 连接池 */
static WsConnection* s_connections[MAX_WS_CONNECTIONS];
static size_t s_connection_count = 0;
static mutex_t s_ws_lock;

/* ============================================================
 * SHA-1 实现 (用于 WebSocket 握手)
 * ============================================================ */
typedef struct {
    uint32_t state[5];
    uint64_t count;
    uint8_t buffer[64];
} SHA1_CTX;

#define SHA1_ROTL(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

static void sha1_transform(SHA1_CTX* ctx, const uint8_t* data)
{
    uint32_t w[80];
    for (int i = 0; i < 16; i++) {
        w[i] = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
               ((uint32_t)data[2] << 8)  | (uint32_t)data[3];
        data += 4;
    }
    for (int i = 16; i < 80; i++) {
        w[i] = SHA1_ROTL(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1);
    }

    uint32_t a = ctx->state[0], b = ctx->state[1], c = ctx->state[2];
    uint32_t d = ctx->state[3], e = ctx->state[4];

    for (int i = 0; i < 80; i++) {
        uint32_t f, k;
        if (i < 20)       { f = (b & c) | (~b & d); k = 0x5A827999; }
        else if (i < 40)  { f = b ^ c ^ d;          k = 0x6ED9EBA1; }
        else if (i < 60)  { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
        else               { f = b ^ c ^ d;          k = 0xCA62C1D6; }

        uint32_t temp = SHA1_ROTL(a, 5) + f + e + k + w[i];
        e = d; d = c; c = SHA1_ROTL(b, 30); b = a; a = temp;
    }

    ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c;
    ctx->state[3] += d; ctx->state[4] += e;
}

static void sha1_init(SHA1_CTX* ctx)
{
    ctx->state[0] = 0x67452301; ctx->state[1] = 0xEFCDAB89;
    ctx->state[2] = 0x98BADCFE; ctx->state[3] = 0x10325476;
    ctx->state[4] = 0xC3D2E1F0;
    ctx->count = 0;
}

static void sha1_update(SHA1_CTX* ctx, const uint8_t* data, size_t len)
{
    size_t idx = (size_t)(ctx->count & 63);
    ctx->count += (uint64_t)len;

    size_t part = 64 - idx;
    if (len >= part) {
        memcpy(ctx->buffer + idx, data, part);
        sha1_transform(ctx, ctx->buffer);
        data += part; len -= part;
        while (len >= 64) {
            sha1_transform(ctx, data);
            data += 64; len -= 64;
        }
        idx = 0;
    }
    memcpy(ctx->buffer + idx, data, len);
}

static void sha1_final(SHA1_CTX* ctx, uint8_t digest[20])
{
    uint64_t bits = ctx->count * 8;
    uint8_t pad = 0x80;
    sha1_update(ctx, &pad, 1);

    while ((ctx->count & 63) != 56) {
        pad = 0x00;
        sha1_update(ctx, &pad, 1);
    }

    for (int i = 0; i < 8; i++) {
        pad = (uint8_t)(bits >> (56 - i * 8));
        sha1_update(ctx, &pad, 1);
    }

    for (int i = 0; i < 5; i++) {
        digest[i*4+0] = (uint8_t)(ctx->state[i] >> 24);
        digest[i*4+1] = (uint8_t)(ctx->state[i] >> 16);
        digest[i*4+2] = (uint8_t)(ctx->state[i] >> 8);
        digest[i*4+3] = (uint8_t)(ctx->state[i] >> 0);
    }
}

/* ============================================================
 * Base64 编码
 * ============================================================ */
static const char b64_table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void base64_encode(const uint8_t* input, size_t len, char* output)
{
    size_t i, j;
    for (i = 0, j = 0; i < len; i += 3) {
        uint32_t v = (uint32_t)input[i] << 16;
        if (i + 1 < len) v |= (uint32_t)input[i+1] << 8;
        if (i + 2 < len) v |= (uint32_t)input[i+2];

        output[j++] = b64_table[(v >> 18) & 0x3F];
        output[j++] = b64_table[(v >> 12) & 0x3F];
        output[j++] = (i + 1 < len) ? b64_table[(v >> 6) & 0x3F] : '=';
        output[j++] = (i + 2 < len) ? b64_table[v & 0x3F] : '=';
    }
    output[j] = '\0';
}

/* ============================================================
 * WebSocket 握手
 * ============================================================ */
static int ws_generate_accept_key(const char* client_key, char* accept_key, size_t key_size)
{
    char concat[256];
    snprintf(concat, sizeof(concat), "%s%s", client_key, WS_GUID);

    SHA1_CTX sha1;
    uint8_t digest[20];
    sha1_init(&sha1);
    sha1_update(&sha1, (const uint8_t*)concat, strlen(concat));
    sha1_final(&sha1, digest);

    base64_encode(digest, 20, accept_key);
    return 0;
}

/* ============================================================
 * WS 帧编码/解码
 * ============================================================ */

/* 发送 WebSocket 帧 */
static int ws_send_frame(WsConnection* conn, uint8_t opcode,
                         const uint8_t* data, uint64_t length)
{
    uint8_t header[14];
    size_t header_len;

    header[0] = 0x80 | opcode; /* FIN + opcode */

    if (length < 126) {
        header[1] = (uint8_t)length;
        header_len = 2;
    } else if (length <= 0xFFFF) {
        header[1] = 126;
        header[2] = (uint8_t)(length >> 8);
        header[3] = (uint8_t)(length & 0xFF);
        header_len = 4;
    } else {
        header[1] = 127;
        for (int i = 7; i >= 0; i--) {
            header[2 + (7 - i)] = (uint8_t)(length >> (i * 8));
        }
        header_len = 10;
    }

    /* 发送头部 */
    int sent = (int)send(conn->fd, (const char*)header, header_len, 0);
    if (sent != (int)header_len) return -1;

    /* 发送数据 */
    if (length > 0) {
        sent = (int)send(conn->fd, (const char*)data, (int)length, 0);
        if (sent != (int)length) return -1;
    }

    return 0;
}

/* 接收并处理 WebSocket 帧 */
static int ws_process_frame(WsConnection* conn)
{
    uint8_t header[2];
    int received;

    /* 读取前 2 字节头部 */
    received = (int)recv(conn->fd, (char*)header, 2, 0);
    if (received != 2) return -1;

    bool fin = (header[0] & 0x80) != 0;
    (void)fin; /* 目前未使用，可用于分片重组 */

    uint8_t opcode = header[0] & 0x0F;
    bool masked = (header[1] & 0x80) != 0;
    uint64_t payload_length = header[1] & 0x7F;

    /* 读取扩展长度 */
    if (payload_length == 126) {
        uint8_t len_buf[2];
        if (recv(conn->fd, (char*)len_buf, 2, 0) != 2) return -1;
        payload_length = ((uint64_t)len_buf[0] << 8) | len_buf[1];
    } else if (payload_length == 127) {
        uint8_t len_buf[8];
        if (recv(conn->fd, (char*)len_buf, 8, 0) != 8) return -1;
        payload_length = 0;
        for (int i = 0; i < 8; i++) {
            payload_length = (payload_length << 8) | len_buf[i];
        }
    }

    /* 读取掩码 */
    uint8_t mask[4] = {0};
    if (masked) {
        if (recv(conn->fd, (char*)mask, 4, 0) != 4) return -1;
    }

    /* 读取 payload */
    uint8_t* payload = NULL;
    if (payload_length > 0) {
        payload = (uint8_t*)malloc((size_t)payload_length);
        if (!payload) return -1;

        uint64_t remaining = payload_length;
        uint64_t offset = 0;
        while (remaining > 0) {
            int chunk = (int)(remaining < 65536 ? remaining : 65536);
            received = (int)recv(conn->fd, (char*)(payload + offset), chunk, 0);
            if (received <= 0) {
                free(payload);
                return -1;
            }
            offset += (uint64_t)received;
            remaining -= (uint64_t)received;
        }

        /* 解除掩码 */
        if (masked) {
            for (uint64_t i = 0; i < payload_length; i++) {
                payload[i] ^= mask[i % 4];
            }
        }
    }

    /* 处理控制帧 */
    switch (opcode) {
        case WS_OPCODE_CLOSE: {
            uint16_t close_code = 1000;
            if (payload_length >= 2) {
                close_code = ((uint16_t)payload[0] << 8) | payload[1];
            }
            ws_close(conn, close_code, "Client closed");
            free(payload);
            return 1; /* 表示连接已关闭 */
        }
        case WS_OPCODE_PING: {
            ws_send_frame(conn, WS_OPCODE_PONG, payload, payload_length);
            free(payload);
            return 0;
        }
        case WS_OPCODE_PONG:
            free(payload);
            return 0;
        default:
            break;
    }

    /* 触发数据回调 */
    if (conn->on_message && (opcode == WS_OPCODE_TEXT || opcode == WS_OPCODE_BINARY)) {
        conn->on_message(conn, opcode, payload, (size_t)payload_length);
    }

    free(payload);
    return 0;
}

/* ============================================================
 * API 实现
 * ============================================================ */

int ws_init(void)
{
    mutex_init(&s_ws_lock);
    s_connection_count = 0;
    LOG_INFO("WebSocket 模块初始化完成");
    return 0;
}

int ws_handle_upgrade(const HttpRequest* req, HttpResponse* resp,
                      WsMessageCallback on_msg, WsCloseCallback on_close,
                      WsConnectCallback on_connect, void* user_data)
{
    (void)on_msg;
    (void)on_close;
    (void)on_connect;
    (void)user_data;

    /* 验证 WebSocket 升级请求 */
    const char* upgrade = NULL;
    const char* connection = NULL;
    const char* ws_key = NULL;
    const char* ws_version = NULL;

    for (size_t i = 0; i < req->header_count; i++) {
        if (strcasecmp(req->headers[i][0], "Upgrade") == 0)
            upgrade = req->headers[i][1];
        else if (strcasecmp(req->headers[i][0], "Connection") == 0)
            connection = req->headers[i][1];
        else if (strcasecmp(req->headers[i][0], "Sec-WebSocket-Key") == 0)
            ws_key = req->headers[i][1];
        else if (strcasecmp(req->headers[i][0], "Sec-WebSocket-Version") == 0)
            ws_version = req->headers[i][1];
    }

    if (!upgrade || strcasecmp(upgrade, "websocket") != 0 ||
        !ws_key || !ws_version || atoi(ws_version) < 13) {
        http_response_set_status(resp, 400, "Bad Request");
        http_response_set_json(resp, "{\"status\":\"error\",\"message\":\"Invalid WebSocket upgrade\"}");
        return -1;
    }

    /* 生成 Accept Key */
    char accept_key[64];
    ws_generate_accept_key(ws_key, accept_key, sizeof(accept_key));

    /* 构建 101 响应 */
    http_response_set_status(resp, 101, "Switching Protocols");
    http_response_set_header(resp, "Upgrade", "websocket");
    http_response_set_header(resp, "Connection", "Upgrade");
    http_response_set_header(resp, "Sec-WebSocket-Accept", accept_key);

    return 0;
}

int ws_send(WsConnection* conn, enum WsOpcode opcode, const uint8_t* data, size_t length)
{
    if (!conn || conn->closed) return -1;
    return ws_send_frame(conn, (uint8_t)opcode, data, length);
}

int ws_send_text(WsConnection* conn, const char* text)
{
    return ws_send(conn, WS_OPCODE_TEXT, (const uint8_t*)text, strlen(text));
}

int ws_send_binary(WsConnection* conn, const uint8_t* data, size_t length)
{
    return ws_send(conn, WS_OPCODE_BINARY, data, length);
}

int ws_close(WsConnection* conn, uint16_t code, const char* reason)
{
    if (!conn || conn->closed) return -1;

    /* 发送关闭帧 */
    uint8_t close_frame[4];
    close_frame[0] = (uint8_t)(code >> 8);
    close_frame[1] = (uint8_t)(code & 0xFF);
    ws_send_frame(conn, WS_OPCODE_CLOSE, close_frame, 2);

    conn->closed = true;

    if (conn->on_close) {
        conn->on_close(conn, code, reason ? reason : "");
    }

    /* 从连接池移除 */
    mutex_lock(&s_ws_lock);
    for (size_t i = 0; i < s_connection_count; i++) {
        if (s_connections[i] == conn) {
            s_connections[i] = s_connections[--s_connection_count];
            break;
        }
    }
    mutex_unlock(&s_ws_lock);

    close_socket(conn->fd);
    free(conn);

    return 0;
}

void ws_get_peer_addr(WsConnection* conn, char* addr_buf, size_t buf_size)
{
    if (conn && addr_buf && buf_size > 0) {
        size_t len = strlen(conn->peer_addr);
        size_t copy = (len < buf_size - 1) ? len : (buf_size - 1);
        memcpy(addr_buf, conn->peer_addr, copy);
        addr_buf[copy] = '\0';
    }
}

/* 创建新的 WebSocket 连接（从 socket 升级） */
WsConnection* ws_create_connection(socket_t fd, const struct sockaddr_in* addr,
                                    WsMessageCallback on_msg, WsCloseCallback on_close,
                                    WsConnectCallback on_connect, void* user_data)
{
    WsConnection* conn = (WsConnection*)calloc(1, sizeof(WsConnection));
    if (!conn) return NULL;

    conn->fd = fd;
    conn->on_message = on_msg;
    conn->on_close = on_close;
    conn->on_connect = on_connect;
    conn->user_data = user_data;
    conn->closed = false;

    inet_ntop(AF_INET, &addr->sin_addr, conn->peer_addr, sizeof(conn->peer_addr));
    conn->peer_port = ntohs(addr->sin_port);

    /* 添加到连接池 */
    mutex_lock(&s_ws_lock);
    if (s_connection_count < MAX_WS_CONNECTIONS) {
        s_connections[s_connection_count++] = conn;
    }
    mutex_unlock(&s_ws_lock);

    if (conn->on_connect) {
        conn->on_connect(conn);
    }

    return conn;
}
