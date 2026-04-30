/**
 * Chrono-shift 客户端网络层 - SHA-1 + Base64
 * 语言标准: C99
 *
 * 纯 C 实现的 SHA-1 哈希和 Base64 编码
 * 用于 WebSocket (RFC 6455) 握手密钥生成与验证
 */

#include "net_core.h"
#include <string.h>

/* ============================================================
 * SHA-1 实现
 * ============================================================ */

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

void sha1_init(Sha1Context* ctx)
{
    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xEFCDAB89;
    ctx->state[2] = 0x98BADCFE;
    ctx->state[3] = 0x10325476;
    ctx->state[4] = 0xC3D2E1F0;
    ctx->count = 0;
}

void sha1_update(Sha1Context* ctx, const unsigned char* data, size_t len)
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

void sha1_final(Sha1Context* ctx, unsigned char digest[20])
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

void base64_encode(const unsigned char* input, size_t input_len, char* output)
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
