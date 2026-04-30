#ifndef CHRONO_CLIENT_NET_CORE_H
#define CHRONO_CLIENT_NET_CORE_H

/**
 * Chrono-shift 客户端网络模块内部头文件
 * 语言标准: C99
 *
 * 暴露跨文件共享的内部函数声明
 * (原本在 network.c 中为 static，拆分后需跨模块可见)
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <winsock2.h>
#include <windows.h>

#include "network.h"

/* ============================================================
 * SHA-1 上下文 (WebSocket 握手用)
 * ============================================================ */

typedef struct {
    uint32_t state[5];
    uint64_t count;
    unsigned char buffer[64];
} Sha1Context;

void sha1_init(Sha1Context* ctx);
void sha1_update(Sha1Context* ctx, const unsigned char* data, size_t len);
void sha1_final(Sha1Context* ctx, unsigned char digest[20]);

/* ============================================================
 * Base64 编码 (WebSocket Key 生成)
 * ============================================================ */

void base64_encode(const unsigned char* input, size_t input_len, char* output);

/* ============================================================
 * TCP 收发辅助 (HTTP + WebSocket 共用)
 * ============================================================ */

int tcp_send_all(NetworkContext* ctx, const uint8_t* data, size_t length);
int tcp_recv_all(NetworkContext* ctx, uint8_t* buffer, size_t length);

#endif /* CHRONO_CLIENT_NET_CORE_H */
