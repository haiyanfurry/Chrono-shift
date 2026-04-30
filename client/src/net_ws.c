/**
 * Chrono-shift 客户端网络层 - WebSocket
 * 语言标准: C99
 *
 * WebSocket (RFC 6455) 握手、帧编解码
 * 支持 Binary 帧收发，自动处理 Ping/Pong/Close 控制帧
 */

#include "net_core.h"
#include "client.h"
#include <stdio.h>

/* TLS 抽象层 */
#include "../../server/include/tls_server.h"
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * 常量
 * ============================================================ */

#define WS_KEY_LEN        24    /* base64(16 bytes) = 24 chars + padding */
#define WS_ACCEPT_LEN     28    /* base64(sha1(20 bytes)) = 28 chars */
#define WS_GUID           "258EAFA5-E914-47DA-95CA-5AB5E9A1DA32"

/* ============================================================
 * WebSocket 连接握手
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
                     "GET %s HTTP/1.1\r\n"
                     "Host: %s:%d\r\n"
                     "Upgrade: websocket\r\n"
                     "Connection: Upgrade\r\n"
                     "Sec-WebSocket-Key: %s\r\n"
                     "Sec-WebSocket-Version: 13\r\n"
                     "\r\n",
                     path,
                     ctx->server_host, ctx->server_port,
                     key_b64);

    if (n < 0 || (size_t)n >= sizeof(request)) {
        LOG_ERROR("WebSocket 请求头过长");
        return -1;
    }

    if (tcp_send_all(ctx, (uint8_t*)request, (size_t)n) != 0) {
        LOG_ERROR("发送 WebSocket 握手请求失败");
        return -1;
    }

    /* --- 接收响应 --- */
    uint8_t response[1024];
    size_t resp_len = 0;

    while (resp_len < sizeof(response) - 1) {
        int r;
        if (ctx->ssl) {
            r = (int)tls_read((SSL*)ctx->ssl, response + resp_len,
                              (int)(sizeof(response) - resp_len - 1));
        } else {
            r = recv(ctx->sock, (char*)(response + resp_len),
                     (int)(sizeof(response) - resp_len - 1), 0);
        }
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
 * WebSocket 帧发送
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
    int result = tcp_send_all(ctx, send_buf, header_len + length);
    free(send_buf);

    if (result != 0) {
        LOG_ERROR("WebSocket 发送失败");
        ctx->connected = false;
        return -1;
    }

    return 0;
}

/* ============================================================
 * WebSocket 帧接收
 * ============================================================ */

int net_ws_recv(NetworkContext* ctx, uint8_t* buffer, size_t buf_size, size_t* received)
{
    if (!ctx || !ctx->connected || !buffer || !received) {
        return -1;
    }

    *received = 0;

    /* --- 读取 Byte 0 (FIN + RSV + Opcode) --- */
    uint8_t byte0;
    if (tcp_recv_all(ctx, &byte0, 1) != 0) {
        ctx->connected = false;
        return -1;
    }

    uint8_t fin     = (byte0 >> 7) & 0x01;
    uint8_t opcode  = byte0 & 0x0F;

    (void)fin; /* 暂不使用 */

    /* 处理控制帧 */
    if (opcode == 0x8) { /* Close */
        uint8_t close_hdr[2];
        if (tcp_recv_all(ctx, close_hdr, 2) != 0) return -1;
        uint16_t close_code = ((uint16_t)close_hdr[0] << 8) | close_hdr[1];
        LOG_DEBUG("WebSocket 收到 Close 帧, code=%d", close_code);
        ctx->connected = false;
        return -1; /* 连接关闭 */
    } else if (opcode == 0x9) { /* Ping */
        /* 回复 Pong */
        uint8_t pong[] = { 0x8A, 0x00 };
        if (ctx->ssl) {
            tls_write((SSL*)ctx->ssl, pong, 2);
        } else {
            send(ctx->sock, (const char*)pong, 2, 0);
        }
        return net_ws_recv(ctx, buffer, buf_size, received); /* 递归读取下一帧 */
    } else if (opcode == 0xA) { /* Pong */
        /* 忽略 Pong，继续读取 */
        uint8_t ignore_hdr;
        tcp_recv_all(ctx, &ignore_hdr, 1);
        return net_ws_recv(ctx, buffer, buf_size, received);
    }

    /* --- 读取 Byte 1 (Mask + Payload Length) --- */
    uint8_t byte1;
    if (tcp_recv_all(ctx, &byte1, 1) != 0) {
        ctx->connected = false;
        return -1;
    }

    uint8_t masked      = (byte1 >> 7) & 0x01;
    uint64_t payload_len = byte1 & 0x7F;

    /* 扩展长度 */
    if (payload_len == 126) {
        uint8_t ext[2];
        if (tcp_recv_all(ctx, ext, 2) != 0) return -1;
        payload_len = ((uint64_t)ext[0] << 8) | ext[1];
    } else if (payload_len == 127) {
        uint8_t ext[8];
        if (tcp_recv_all(ctx, ext, 8) != 0) return -1;
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
        if (tcp_recv_all(ctx, mask_key, 4) != 0) return -1;
    }

    /* 读取 payload */
    if (payload_len > 0) {
        if (tcp_recv_all(ctx, buffer, (size_t)payload_len) != 0) {
            ctx->connected = false;
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
