/**
 * Chrono-shift 客户端 SHA-1 + Base64 工具实现
 * C++17 重构版 (从 C99 net_sha1.c 移植)
 */
#include "Sha1.h"

#include <cstring>
#include <cstdio>

namespace chrono {
namespace client {
namespace network {

// ============================================================
// SHA-1 常量与内部函数
// ============================================================

namespace {

/** SHA-1 轮常量 */
const uint32_t K[] = {
    0x5A827999, 0x6ED9EBA1, 0x8F1BBCDC, 0xCA62C1D6
};

/** 循环左移 */
inline uint32_t left_rotate(uint32_t value, uint32_t bits)
{
    return (value << bits) | (value >> (32 - bits));
}

/** Base64 编码表 */
const char BASE64_TABLE[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

} // anonymous namespace

// ============================================================
// Sha1 实现
// ============================================================

Sha1::Sha1()
{
    init();
}

void Sha1::init()
{
    state_[0] = 0x67452301;
    state_[1] = 0xEFCDAB89;
    state_[2] = 0x98BADCFE;
    state_[3] = 0x10325476;
    state_[4] = 0xC3D2E1F0;
    count_ = 0;
    std::memset(buffer_, 0, sizeof(buffer_));
}

void Sha1::update(const uint8_t* data, size_t len)
{
    if (!data || len == 0) return;

    size_t index = static_cast<size_t>((count_ >> 3) & 0x3F);
    count_ += static_cast<uint64_t>(len) << 3;

    size_t free_space = 64 - index;
    size_t processed = 0;

    if (len >= free_space) {
        std::memcpy(buffer_ + index, data, free_space);
        // 处理一个块
        {
            uint32_t w[80];
            for (int t = 0; t < 16; t++) {
                w[t] = (static_cast<uint32_t>(buffer_[t * 4]) << 24) |
                       (static_cast<uint32_t>(buffer_[t * 4 + 1]) << 16) |
                       (static_cast<uint32_t>(buffer_[t * 4 + 2]) << 8) |
                       static_cast<uint32_t>(buffer_[t * 4 + 3]);
            }
            for (int t = 16; t < 80; t++) {
                w[t] = left_rotate(w[t - 3] ^ w[t - 8] ^ w[t - 14] ^ w[t - 16], 1);
            }

            uint32_t a = state_[0], b = state_[1], c = state_[2];
            uint32_t d = state_[3], e = state_[4];

            for (int t = 0; t < 80; t++) {
                uint32_t f, k;
                if (t < 20) {
                    f = (b & c) | ((~b) & d);
                    k = K[0];
                } else if (t < 40) {
                    f = b ^ c ^ d;
                    k = K[1];
                } else if (t < 60) {
                    f = (b & c) | (b & d) | (c & d);
                    k = K[2];
                } else {
                    f = b ^ c ^ d;
                    k = K[3];
                }

                uint32_t temp = left_rotate(a, 5) + f + e + k + w[t];
                e = d;
                d = c;
                c = left_rotate(b, 30);
                b = a;
                a = temp;
            }

            state_[0] += a;
            state_[1] += b;
            state_[2] += c;
            state_[3] += d;
            state_[4] += e;
        }

        processed = free_space;
        while (processed + 63 < len) {
            // 处理后续块
            uint32_t w[80];
            for (int t = 0; t < 16; t++) {
                w[t] = (static_cast<uint32_t>(data[processed + t * 4]) << 24) |
                       (static_cast<uint32_t>(data[processed + t * 4 + 1]) << 16) |
                       (static_cast<uint32_t>(data[processed + t * 4 + 2]) << 8) |
                       static_cast<uint32_t>(data[processed + t * 4 + 3]);
            }
            for (int t = 16; t < 80; t++) {
                w[t] = left_rotate(w[t - 3] ^ w[t - 8] ^ w[t - 14] ^ w[t - 16], 1);
            }

            uint32_t a = state_[0], b = state_[1], c = state_[2];
            uint32_t d = state_[3], e = state_[4];

            for (int t = 0; t < 80; t++) {
                uint32_t f, k;
                if (t < 20) {
                    f = (b & c) | ((~b) & d);
                    k = K[0];
                } else if (t < 40) {
                    f = b ^ c ^ d;
                    k = K[1];
                } else if (t < 60) {
                    f = (b & c) | (b & d) | (c & d);
                    k = K[2];
                } else {
                    f = b ^ c ^ d;
                    k = K[3];
                }

                uint32_t temp = left_rotate(a, 5) + f + e + k + w[t];
                e = d;
                d = c;
                c = left_rotate(b, 30);
                b = a;
                a = temp;
            }

            state_[0] += a;
            state_[1] += b;
            state_[2] += c;
            state_[3] += d;
            state_[4] += e;

            processed += 64;
        }

        index = 0;
    }

    std::memcpy(buffer_ + index, data + processed, len - processed);
}

void Sha1::final(uint8_t digest[20])
{
    if (!digest) return;

    uint64_t bits = count_;
    size_t index = static_cast<size_t>((count_ >> 3) & 0x3F);

    // 填充: 追加 0x80
    uint8_t padding_buf[128];
    size_t pad_len = (index < 56) ? (56 - index) : (120 - index);
    std::memset(padding_buf, 0, pad_len);
    padding_buf[0] = 0x80;

    update(padding_buf, pad_len);

    // 追加原始位计数 (大端序)
    uint8_t bits_buf[8];
    for (int i = 0; i < 8; i++) {
        bits_buf[i] = static_cast<uint8_t>((bits >> (56 - i * 8)) & 0xFF);
    }
    update(bits_buf, 8);

    // 输出摘要
    for (int i = 0; i < 5; i++) {
        digest[i * 4]     = static_cast<uint8_t>((state_[i] >> 24) & 0xFF);
        digest[i * 4 + 1] = static_cast<uint8_t>((state_[i] >> 16) & 0xFF);
        digest[i * 4 + 2] = static_cast<uint8_t>((state_[i] >> 8) & 0xFF);
        digest[i * 4 + 3] = static_cast<uint8_t>(state_[i] & 0xFF);
    }
}

void Sha1::base64_encode(const uint8_t* input, size_t input_len, char* output)
{
    if (!input || !output) return;

    size_t i = 0, o = 0;
    while (i < input_len) {
        uint32_t octet_a = (i < input_len) ? input[i++] : 0;
        uint32_t octet_b = (i < input_len) ? input[i++] : 0;
        uint32_t octet_c = (i < input_len) ? input[i++] : 0;

        uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;

        output[o++] = BASE64_TABLE[(triple >> 18) & 0x3F];
        output[o++] = BASE64_TABLE[(triple >> 12) & 0x3F];
        output[o++] = BASE64_TABLE[(triple >> 6) & 0x3F];
        output[o++] = BASE64_TABLE[triple & 0x3F];
    }

    // 填充 '='
    size_t padding = (3 - (input_len % 3)) % 3;
    for (size_t p = 0; p < padding; p++) {
        output[o - 1 - p] = '=';
    }
    output[o] = '\0';
}

std::string Sha1::hash(const std::string& data)
{
    Sha1 sha1;
    sha1.update(reinterpret_cast<const uint8_t*>(data.data()), data.size());
    uint8_t digest[20];
    sha1.final(digest);

    char hex[41];
    for (int i = 0; i < 20; i++) {
        std::sprintf(hex + i * 2, "%02x", digest[i]);
    }
    hex[40] = '\0';
    return std::string(hex);
}

} // namespace network
} // namespace client
} // namespace chrono
