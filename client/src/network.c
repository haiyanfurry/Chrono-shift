/**
 * Chrono-shift 客户端网络层
 * 语言标准: C99
 *
 * Winsock 2.2 实现的 HTTP/1.1 和 WebSocket (RFC 6455) 客户端
 * 功能: TCP 连接、HTTP 请求/响应、WebSocket 握手/帧编解码
 */

#include "network.h"
#include "client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>

/* ============================================================
 * 常量定义
 * ============================================================ */

#define NET_BUF_SIZE      65536
#define WS_KEY_LEN        24    /* base64(16 bytes) = 24 chars + padding */
#define WS_ACCEPT_LEN     28    /* base64(sha1(20 bytes)) = 28 chars */

#define HTTP_PROTO        "HTTP/1.1"
#define WS_GUID           "258EAFA5-E914-47DA-95CA-5AB5E9A1DA32"

/* ============================================================
 * SHA-1 实现 (纯 C, 用于 WebSocket 握手)
 * ============================================================ */

typedef struct {
    uint32_t state[5];
    uint64_t count;
    unsigned char buffer[64];
} Sha1Context;

#define SHA1_ROTL(x, n)  (((x) << (n)) | ((x) >> (32 - (n))))

static void sha1_transform(uint32_t state[5], const unsigned char block[64])
{
    uint32_t w[80];
    uint32_t a, b, c, d, e, temp;

    for (int i = 0; i < 16; i++) {
        w[i] = ((uint32_t)block[i * 4] << 24)
             | ((uint32_t)block[i * 4 + 1] << 16)
             | ((uint32_t)block[i * 4 + 2] << 8)
             | ((uint32_t)block[i * 4 + 3]);
    }
    for (int i = 16; i < 80; i++) {
        w[i] = SHA1_ROTL(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1);
    }

    a = state[0]; b = state[1]; c = state[2]; d = state[3]; e = state[4];

    #define SHA1_ROUND(f, k) do { \
        temp = SHA1_ROTL(a, 5) + (f) + e + (k) + w[i]; \
        e = d; d = c; c = SHA1_ROTL(b, 30); b = a; a = temp; \
    } while (0)

    for (int i = 0; i < 20; i++)
        SHA1_ROUND((b & c) | (~b & d), 0x5A827999);
    for (int i = 20; i < 40; i++)
        SHA1_ROUND(b ^ c ^ d, 0x6ED9EBA1);
    for (int i = 40; i < 60; i++)
        SHA1_ROUND((b & c) | (b & d) | (c & d), 0x8F1BBCDC);
    for (int i = 60; i < 80; i++)
        SHA1_ROUND(b ^ c ^ d, 0xCA62C1D6);

    #undef SHA1_ROUND

    state[0] += a; state[1] += b; state[2] += c; state[3] += d; state[4] += e;
}

static void sha1_init(Sha1Context* ctx)
{
    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xEFCDAB89;
    ctx->state[2] = 0x98BADCFE;
    ctx->state[3] = 0x10325476;
    ctx->state[4] = 0xC3D2E1F0;
    ctx->count = 0;
}

static void sha1_update(Sha1Context* ctx, const unsigned char* data, size_t len)
{
    size_t idx = (size_t)(ctx->count & 63);
    ctx->count += (uint64_t)len;
    size_t part = 64 - idx;

    if (len >= part) {
        memcpy(ctx->buffer + idx, data, part);
        sha1_transform(ctx->state, ctx->buffer);
        for (size_t i = part; i + 63 < len; i += 64) {
            sha1_transform(ctx->state, data + i);
        }
        idx = 0;
    } else {
        part = len;
    }
    memcpy(ctx->buffer + idx, data + (len - part), part);
}

static void sha1_final(Sha1Context* ctx, unsigned char digest[20])
{
    size_t idx = (size_t)(ctx->count & 63);
    size_t pad_len = (idx < 56) ? (56 - idx) : (120 - idx);

    unsigned char pad[128];
    memset(pad, 0, pad_len);
    pad[0] = 0x80;

    uint64_t bits = ctx->count * 8;
    sha1_update(ctx, pad, pad_len);

    unsigned char count_bytes[8];
    for (int i = 0; i < 8; i++) {
        count_bytes[i] = (unsigned char)(bits >> (56 - i * 8));
    }
    sha1_update(ctx, count_bytes, 8);

    for (int i = 0; i < 5; i++) {
        digest[i * 4]     = (unsigned char)(ctx->state[i] >> 24);
        digest[i * 4 + 1] = (unsigned char)(ctx->state[i] >> 16);
        digest[i * 4 + 2] = (unsigned char)(ctx->state[i] >> 8);
        digest[i * 4 + 3] = (unsigned char)(ctx->state[i]);
    }
}

/* ============================================================
 * Base64 编码 (用于 WebSocket Key)
 * ============================================================ */

static const char BASE64_TABLE[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void base64_encode(const unsigned char* input, size_t input_len, char* output)
{
    size_t i, j = 0;
    for (i = 0; i < input_len; i += 3) {
        unsigned int val = (unsigned int)input[i] << 16;
        if (i + 1 < input_len) val |= (unsigned int)input[i+1] << 8;
        if (i + 2 < input_len) val |= (unsigned int)input[i+2];

        output[j++] = BASE64_TABLE[(val >> 18) & 0x3F];
        output[j++] = BASE64_TABLE[(val >> 12) & 0x3F];
        output[j++] = (i + 1 < input_len) ? BASE64_TABLE[(val >> 6) & 0x3F] : '=';
        output[j++] = (i + 2 < input_len) ? BASE64_TABLE[val & 0x3F] : '=';
    }
    output[j] = '\0';
}

/* ============================================================
 * Winsock 初始化/清理
 * ============================================================ */

static int g_winsock_count = 0;

int net_init(NetworkContext* ctx)
{
    memset(ctx, 0, sizeof(NetworkContext));
    ctx->sock = -1;

    if (g_winsock_count == 0) {
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
            LOG_ERROR("Winsock 初始化失败");
            return -1;
        }
    }
    g_winsock_count++;

    LOG_INFO("网络模块初始化完成");
    return 0;
}

void net_cleanup(void)
{
    if (g_winsock_count > 0) {
        g_winsock_count--;
        if (g_winsock_count == 0) {
            WSACleanup();
        }
    }
}

/* ============================================================
 * TCP 连接管理
 * ============================================================ */

int net_connect(NetworkContext* ctx, const char* host, uint16_t port)
{
    if (!ctx || !host) return -1;

    /* 如果已有连接，先断开 */
    if (ctx->connected) {
        net_disconnect(ctx);
    }

    /* 创建 socket */
    ctx->sock = (int)socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ctx->sock < 0) {
        LOG_ERROR("socket() 失败: %d", WSAGetLastError());
        return -1;
    }

    /* 设置超时 */
    int timeout = 10000; /* 10秒 */
    setsockopt(ctx->sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    setsockopt(ctx->sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));

    /* 解析服务器地址 */
    struct hostent* he = gethostbyname(host);
    if (!he) {
        LOG_ERROR("gethostbyname() 失败: %s", host);
        closesocket(ctx->sock);
        ctx->sock = -1;
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);

    /* 连接 */
    if (connect(ctx->sock, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        LOG_ERROR("connect() 失败: %d", WSAGetLastError());
        closesocket(ctx->sock);
        ctx->sock = -1;
        return -1;
    }

    strncpy(ctx->server_host, host, sizeof(ctx->server_host) - 1);
    ctx->server_port = port;
    ctx->connected = true;

    LOG_INFO("已连接到服务器: %s:%d", host, port);
    return 0;
}

void net_disconnect(NetworkContext* ctx)
{
    if (ctx && ctx->connected) {
        closesocket(ctx->sock);
        ctx->sock = -1;
        ctx->connected = false;
        LOG_INFO("网络连接已断开");
    }
}

bool net_is_connected(NetworkContext* ctx)
{
    return ctx ? ctx->connected : false;
}

/* ============================================================
 * TCP 收发辅助
 * ============================================================ */

static int tcp_send_all(SOCKET sock, const uint8_t* data, size_t length)
{
    size_t sent = 0;
    while (sent < length) {
        int n = send(sock, (const char*)(data + sent), (int)(length - sent), 0);
        if (n <= 0) {
            return -1;
        }
        sent += (size_t)n;
    }
    return 0;
}

static int tcp_recv_all(SOCKET sock, uint8_t* buffer, size_t length)
{
    size_t received = 0;
    while (received < length) {
        int n = recv(sock, (char*)(buffer + received), (int)(length - received), 0);
        if (n <= 0) {
            return -1;
        }
        received += (size_t)n;
    }
    return 0;
}

/* ============================================================
 * HTTP 请求/响应
 * ============================================================ */

int net_http_request(NetworkContext* ctx, const char* method, const char* path,
                     const char* headers, const uint8_t* body, size_t body_len,
                     char** response_body, size_t* response_len)
{
    if (!ctx || !ctx->connected || !method || !path) {
        return -1;
    }

    /* 构建 HTTP 请求 */
    char request[NET_BUF_SIZE];
    int n;

    if (headers) {
        if (body && body_len > 0) {
            n = snprintf(request, sizeof(request),
                         "%s %s %s\r\n"
                         "Host: %s\r\n"
                         "Content-Length: %zu\r\n"
                         "%s\r\n"
                         "\r\n",
                         method, path, HTTP_PROTO,
                         ctx->server_host,
                         body_len,
                         headers);
        } else {
            n = snprintf(request, sizeof(request),
                         "%s %s %s\r\n"
                         "Host: %s\r\n"
                         "%s\r\n"
                         "\r\n",
                         method, path, HTTP_PROTO,
                         ctx->server_host,
                         headers);
        }
    } else {
        if (body && body_len > 0) {
            n = snprintf(request, sizeof(request),
                         "%s %s %s\r\n"
                         "Host: %s\r\n"
                         "Content-Length: %zu\r\n"
                         "\r\n",
                         method, path, HTTP_PROTO,
                         ctx->server_host,
                         body_len);
        } else {
            n = snprintf(request, sizeof(request),
                         "%s %s %s\r\n"
                         "Host: %s\r\n"
                         "\r\n",
                         method, path, HTTP_PROTO,
                         ctx->server_host);
        }
    }

    if (n < 0 || (size_t)n >= sizeof(request)) {
        LOG_ERROR("HTTP 请求头过长");
        return -1;
    }

    /* 发送请求头 */
    if (tcp_send_all(ctx->sock, (uint8_t*)request, (size_t)n) != 0) {
        LOG_ERROR("发送 HTTP 请求失败");
        return -1;
    }

    /* 发送请求体 */
    if (body && body_len > 0) {
        if (tcp_send_all(ctx->sock, body, body_len) != 0) {
            LOG_ERROR("发送 HTTP body 失败");
            return -1;
        }
    }

    /* --- 接收响应 --- */
    uint8_t buffer[NET_BUF_SIZE];
    size_t buf_pos = 0;
    int in_body = 0;
    size_t content_length = 0;
    size_t body_received = 0;
    char* body_start = NULL;

    while (buf_pos < sizeof(buffer) - 1) {
        int n_recv = recv(ctx->sock, (char*)(buffer + buf_pos),
                          (int)(sizeof(buffer) - buf_pos - 1), 0);
        if (n_recv <= 0) {
            break;
        }
        buf_pos += (size_t)n_recv;
        buffer[buf_pos] = '\0';

        if (!in_body) {
            /* 查找 \r\n\r\n 分隔符 */
            char* header_end = strstr((char*)buffer, "\r\n\r\n");
            if (header_end) {
                in_body = 1;
                body_start = header_end + 4;
                body_received = buf_pos - (size_t)(body_start - (char*)buffer);

                /* 解析 Content-Length */
                char* cl = strstr((char*)buffer, "Content-Length:");
                if (cl) {
                    cl += 16;
                    while (*cl == ' ') cl++;
                    content_length = (size_t)atol(cl);
                } else {
                    /* 没有 Content-Length，读取所有数据 */
                    content_length = buf_pos;
                }
            }
        }

        if (in_body) {
            if (content_length > 0 && body_received >= content_length) {
                break;
            }
        }
    }

    if (!in_body) {
        LOG_ERROR("HTTP 响应不完整");
        return -1;
    }

    /* 提取 body */
    size_t actual_body_len = body_received;
    if (content_length > 0 && body_received > content_length) {
        actual_body_len = content_length;
    } else if (content_length > 0) {
        actual_body_len = buf_pos - (size_t)(body_start - (char*)buffer);
        if (actual_body_len > content_length) {
            actual_body_len = content_length;
        }
    } else {
        actual_body_len = buf_pos - (size_t)(body_start - (char*)buffer);
    }

    if (response_body && response_len) {
        *response_body = (char*)malloc(actual_body_len + 1);
        if (*response_body) {
            memcpy(*response_body, body_start, actual_body_len);
            (*response_body)[actual_body_len] = '\0';
            *response_len = actual_body_len;
        } else {
            *response_len = 0;
        }
    }

    return 0;
}

/* ============================================================
 * WebSocket 连接
 * ============================================================ */

int net_ws_connect(NetworkContext* ctx, const char* path)
{
    if (!ctx || !ctx->connected || !path) {
        return -1;
    }

    /* --- 生成随机的 Sec-WebSocket-Key --- */
    unsigned char random_key[16];
    char key_b64[WS_KEY_LEN + 1];
    char expected_accept[WS_ACCEPT_LEN + 1];

    /* 使用 Windows 的加密随机数生成器 */
    HCRYPTPROV hProv = 0;
    if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL,
                             CRYPT_VERIFYCONTEXT)) {
        /* 回退: 使用时间和 rand() */
        srand((unsigned int)time(NULL));
        for (int i = 0; i < 16; i++) {
            random_key[i] = (unsigned char)(rand() & 0xFF);
        }
    } else {
        CryptGenRandom(hProv, 16, random_key);
        CryptReleaseContext(hProv, 0);
    }

    base64_encode(random_key, 16, key_b64);

    /* --- 发送 HTTP Upgrade 请求 --- */
    char request[512];
    int n = snprintf(request, sizeof(request),
                     "GET %s %s\r\n"
                     "Host: %s:%d\r\n"
                     "Upgrade: websocket\r\n"
                     "Connection: Upgrade\r\n"
                     "Sec-WebSocket-Key: %s\r\n"
                     "Sec-WebSocket-Version: 13\r\n"
                     "\r\n",
                     path, HTTP_PROTO,
                     ctx->server_host, ctx->server_port,
                     key_b64);

    if (n < 0 || (size_t)n >= sizeof(request)) {
        LOG_ERROR("WebSocket 请求头过长");
        return -1;
    }

    if (tcp_send_all(ctx->sock, (uint8_t*)request, (size_t)n) != 0) {
        LOG_ERROR("发送 WebSocket 握手请求失败");
        return -1;
    }

    /* --- 接收响应 --- */
    uint8_t response[1024];
    size_t resp_len = 0;

    while (resp_len < sizeof(response) - 1) {
        int r = recv(ctx->sock, (char*)(response + resp_len),
                     (int)(sizeof(response) - resp_len - 1), 0);
        if (r <= 0) {
            LOG_ERROR("WebSocket 握手响应接收失败");
            return -1;
        }
        resp_len += (size_t)r;
        response[resp_len] = '\0';

        /* 检查是否收到完整的响应头 */
        if (strstr((char*)response, "\r\n\r\n")) {
            break;
        }
    }

    response[resp_len] = '\0';

    /* --- 验证响应 --- */
    /* 检查状态码 101 */
    if (!strstr((char*)response, "101")) {
        LOG_ERROR("WebSocket 握手失败: 状态码非 101");
        LOG_DEBUG("响应: %s", (char*)response);
        return -1;
    }

    /* 检查 Upgrade */
    if (!strstr((char*)response, "Upgrade: websocket") &&
        !strstr((char*)response, "upgrade: websocket")) {
        LOG_ERROR("WebSocket 握手失败: 缺少 Upgrade 头");
        return -1;
    }

    /* 验证 Sec-WebSocket-Accept */
    /* 计算期望值: base64(sha1(key + GUID)) */
    {
        char concat[WS_KEY_LEN + 1 + 36]; /* key + GUID */
        int clen = snprintf(concat, sizeof(concat), "%s%s", key_b64, WS_GUID);

        unsigned char sha1_digest[20];
        Sha1Context sha1_ctx;
        sha1_init(&sha1_ctx);
        sha1_update(&sha1_ctx, (unsigned char*)concat, (size_t)clen);
        sha1_final(&sha1_ctx, sha1_digest);

        base64_encode(sha1_digest, 20, expected_accept);
    }

    /* 从响应中提取 Sec-WebSocket-Accept */
    char* accept_hdr = strstr((char*)response, "Sec-WebSocket-Accept:");
    if (!accept_hdr) {
        LOG_ERROR("WebSocket 握手失败: 缺少 Sec-WebSocket-Accept");
        return -1;
    }
    accept_hdr += 21; /* 跳过 "Sec-WebSocket-Accept: " */
    while (*accept_hdr == ' ') accept_hdr++;

    /* 提取到行尾 */
    char received_accept[WS_ACCEPT_LEN + 1];
    int ai = 0;
    while (*accept_hdr && *accept_hdr != '\r' && *accept_hdr != '\n' && ai < WS_ACCEPT_LEN) {
        received_accept[ai++] = *accept_hdr++;
    }
    received_accept[ai] = '\0';

    if (strcmp(received_accept, expected_accept) != 0) {
        LOG_ERROR("WebSocket 握手失败: Sec-WebSocket-Accept 不匹配");
        LOG_DEBUG("期望: %s", expected_accept);
        LOG_DEBUG("收到: %s", received_accept);
        return -1;
    }

    LOG_INFO("WebSocket 连接成功: %s", path);
    return 0;
}

/* ============================================================
 * WebSocket 帧收发
 * ============================================================ */

int net_ws_send(NetworkContext* ctx, const uint8_t* data, size_t length)
{
    if (!ctx || !ctx->connected || !data) {
        return -1;
    }

    /* 构建 WebSocket 帧 (带掩码, 因为客户端必须掩码) */
    uint8_t header[10]; /* 最多 10 字节头部 */
    size_t header_len;

    /* Byte 0: FIN=1, RSV=0, Opcode=0x2 (binary) */
    header[0] = 0x82; /* FIN + Opcode binary */

    /* Byte 1+: 掩码位=1 + 长度编码 */
    if (length < 126) {
        header[1] = 0x80 | (uint8_t)length;
        header_len = 2;
    } else if (length <= 65535) {
        header[1] = 0x80 | 126;
        header[2] = (uint8_t)((length >> 8) & 0xFF);
        header[3] = (uint8_t)(length & 0xFF);
        header_len = 4;
    } else {
        header[1] = 0x80 | 127;
        uint64_t len64 = (uint64_t)length;
        for (int i = 7; i >= 0; i--) {
            header[2 + (7 - i)] = (uint8_t)((len64 >> (i * 8)) & 0xFF);
        }
        header_len = 10;
    }

    /* 生成掩码 key */
    unsigned char mask_key[4];
    HCRYPTPROV hProv = 0;
    if (CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL,
                             CRYPT_VERIFYCONTEXT)) {
        CryptGenRandom(hProv, 4, mask_key);
        CryptReleaseContext(hProv, 0);
    } else {
        mask_key[0] = (unsigned char)(rand() & 0xFF);
        mask_key[1] = (unsigned char)(rand() & 0xFF);
        mask_key[2] = (unsigned char)(rand() & 0xFF);
        mask_key[3] = (unsigned char)(rand() & 0xFF);
    }

    /* 追加掩码 key 到头部 */
    for (int i = 0; i < 4; i++) {
        header[header_len + i] = mask_key[i];
    }
    header_len += 4;

    /* 分配发送缓冲区 = header + 掩码后的数据 */
    uint8_t* send_buf = (uint8_t*)malloc(header_len + length);
    if (!send_buf) return -1;

    memcpy(send_buf, header, header_len);

    /* 对 payload 做掩码处理 */
    for (size_t i = 0; i < length; i++) {
        send_buf[header_len + i] = data[i] ^ mask_key[i % 4];
    }

    /* 发送 */
    int result = tcp_send_all(ctx->sock, send_buf, header_len + length);
    free(send_buf);

    if (result != 0) {
        LOG_ERROR("WebSocket 发送失败");
        return -1;
    }

    return 0;
}

int net_ws_recv(NetworkContext* ctx, uint8_t* buffer, size_t buf_size, size_t* received)
{
    if (!ctx || !ctx->connected || !buffer || !received) {
        return -1;
    }

    *received = 0;

    /* --- 读取 Byte 0 (FIN + RSV + Opcode) --- */
    uint8_t byte0;
    if (tcp_recv_all(ctx->sock, &byte0, 1) != 0) {
        return -1;
    }

    uint8_t fin     = (byte0 >> 7) & 0x01;
    uint8_t opcode  = byte0 & 0x0F;

    (void)fin; /* 暂不使用 */

    /* 处理控制帧 */
    if (opcode == 0x8) { /* Close */
        uint8_t close_hdr[2];
        if (tcp_recv_all(ctx->sock, close_hdr, 2) != 0) return -1;
        uint16_t close_code = ((uint16_t)close_hdr[0] << 8) | close_hdr[1];
        LOG_DEBUG("WebSocket 收到 Close 帧, code=%d", close_code);
        return -1; /* 连接关闭 */
    } else if (opcode == 0x9) { /* Ping */
        /* 回复 Pong */
        uint8_t pong[] = { 0x8A, 0x00 };
        send(ctx->sock, (const char*)pong, 2, 0);
        return net_ws_recv(ctx, buffer, buf_size, received); /* 递归读取下一帧 */
    } else if (opcode == 0xA) { /* Pong */
        /* 忽略 Pong，继续读取 */
        uint8_t ignore_hdr;
        tcp_recv_all(ctx->sock, &ignore_hdr, 1);
        return net_ws_recv(ctx, buffer, buf_size, received);
    }

    /* --- 读取 Byte 1 (Mask + Payload Length) --- */
    uint8_t byte1;
    if (tcp_recv_all(ctx->sock, &byte1, 1) != 0) {
        return -1;
    }

    uint8_t masked      = (byte1 >> 7) & 0x01;
    uint64_t payload_len = byte1 & 0x7F;

    /* 扩展长度 */
    if (payload_len == 126) {
        uint8_t ext[2];
        if (tcp_recv_all(ctx->sock, ext, 2) != 0) return -1;
        payload_len = ((uint64_t)ext[0] << 8) | ext[1];
    } else if (payload_len == 127) {
        uint8_t ext[8];
        if (tcp_recv_all(ctx->sock, ext, 8) != 0) return -1;
        payload_len = 0;
        for (int i = 0; i < 8; i++) {
            payload_len = (payload_len << 8) | ext[i];
        }
    }

    if (payload_len > buf_size) {
        LOG_ERROR("WebSocket 帧过大: %llu", (unsigned long long)payload_len);
        return -1;
    }

    /* 读取掩码 key (如果有) */
    unsigned char mask_key[4];
    if (masked) {
        if (tcp_recv_all(ctx->sock, mask_key, 4) != 0) return -1;
    }

    /* 读取 payload */
    if (payload_len > 0) {
        if (tcp_recv_all(ctx->sock, buffer, (size_t)payload_len) != 0) {
            return -1;
        }

        /* 如果服务端发了掩码，解除掩码 */
        if (masked) {
            for (uint64_t i = 0; i < payload_len; i++) {
                buffer[i] ^= mask_key[i % 4];
            }
        }
    }

    *received = (size_t)payload_len;
    return 0;
}
