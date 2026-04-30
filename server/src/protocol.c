/**
 * Chrono-shift 通信协议编解码
 * 语言标准: C99
 *
 * 实现自定义二进制协议 (8字节头部 + JSON body)
 * 协议格式: [magic(4)][ver(1)][type(1)][body_len(2)][body...]
 */

#include "protocol.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

/* ============================================================
 * 头部编解码
 * ============================================================ */

uint16_t protocol_encode_header(uint8_t* buffer, size_t buf_size,
                                uint8_t msg_type, const uint8_t* body, uint16_t body_len)
{
    (void)body;
    if (buf_size < HEADER_SIZE || body_len > 65535) {
        return 0;
    }

    ProtocolHeader* header = (ProtocolHeader*)buffer;
    header->magic = PROTOCOL_MAGIC;
    header->version = PROTOCOL_VERSION;
    header->msg_type = msg_type;
    header->body_length = body_len;

    return HEADER_SIZE;
}

int protocol_decode_header(const uint8_t* buffer, size_t buf_size,
                           ProtocolHeader* header)
{
    if (buf_size < HEADER_SIZE || !buffer || !header) {
        return -1;
    }

    const ProtocolHeader* src = (const ProtocolHeader*)buffer;

    /* 验证魔数 */
    if (src->magic != PROTOCOL_MAGIC) {
        return -1;
    }

    /* 验证版本 */
    if (src->version != PROTOCOL_VERSION) {
        return -1;
    }

    header->magic = src->magic;
    header->version = src->version;
    header->msg_type = src->msg_type;
    header->body_length = src->body_length;

    return 0;
}

/* ============================================================
 * 完整包编解码
 * ============================================================ */

uint16_t protocol_encode_packet(uint8_t* buffer, size_t buf_size,
                                uint8_t msg_type, const uint8_t* body, uint16_t body_len)
{
    uint16_t total_size = HEADER_SIZE + body_len;

    if (buf_size < total_size || body_len > MAX_BODY_SIZE) {
        return 0;
    }

    /* 编码头部 */
    if (protocol_encode_header(buffer, buf_size, msg_type, body, body_len) == 0) {
        return 0;
    }

    /* 复制 body */
    if (body != NULL && body_len > 0) {
        memcpy(buffer + HEADER_SIZE, body, body_len);
    }

    return total_size;
}

int protocol_decode_packet(const uint8_t* buffer, size_t buf_size,
                           ProtocolHeader* header, const uint8_t** body_ptr)
{
    if (!buffer || !header || !body_ptr) {
        return -1;
    }

    /* 至少要有头部大小 */
    if (buf_size < HEADER_SIZE) {
        return -1;
    }

    /* 解码头部 */
    if (protocol_decode_header(buffer, buf_size, header) != 0) {
        return -1;
    }

    /* 检查 body 是否完整 */
    uint16_t body_len = header->body_length;
    if (buf_size < (size_t)HEADER_SIZE + body_len) {
        return -1; /* 数据不完整 */
    }

    /* 指向 body */
    if (body_len > 0) {
        *body_ptr = buffer + HEADER_SIZE;
    } else {
        *body_ptr = NULL;
    }

    return (int)(HEADER_SIZE + body_len);
}

/* ============================================================
 * 辅助函数：构建消息体 (JSON)
 * ============================================================ */

char* protocol_create_heartbeat(void)
{
    char* result = (char*)malloc(64);
    if (!result) return NULL;
    snprintf(result, 64, "{\"type\":\"heartbeat\",\"timestamp\":%lld}",
             (long long)time(NULL));
    return result;
}

char* protocol_create_auth(const char* token)
{
    if (!token) return NULL;
    char* result = (char*)malloc(strlen(token) + 128);
    if (!result) return NULL;
    snprintf(result, strlen(token) + 128,
             "{\"type\":\"auth\",\"token\":\"%s\",\"timestamp\":%lld}",
             token, (long long)time(NULL));
    return result;
}

char* protocol_create_text(const char* from_id, const char* to_id,
                           const char* content, uint64_t timestamp)
{
    if (!from_id || !to_id || !content) return NULL;

    size_t len = strlen(from_id) + strlen(to_id) + strlen(content) + 256;
    char* result = (char*)malloc(len);
    if (!result) return NULL;

    /* 对 content 做简单的 JSON 转义 */
    char* escaped = NULL;
    size_t content_len = strlen(content);
    size_t esc_len = content_len * 2 + 1;
    escaped = (char*)malloc(esc_len);
    if (!escaped) {
        free(result);
        return NULL;
    }

    size_t j = 0;
    for (size_t i = 0; i < content_len && j < esc_len - 4; i++) {
        switch (content[i]) {
            case '"':  escaped[j++] = '\\'; escaped[j++] = '"';  break;
            case '\\': escaped[j++] = '\\'; escaped[j++] = '\\'; break;
            case '\n': escaped[j++] = '\\'; escaped[j++] = 'n';  break;
            case '\r': escaped[j++] = '\\'; escaped[j++] = 'r';  break;
            case '\t': escaped[j++] = '\\'; escaped[j++] = 't';  break;
            default:   escaped[j++] = content[i];                 break;
        }
    }
    escaped[j] = '\0';

    snprintf(result, len,
             "{\"type\":\"text\",\"from_id\":\"%s\",\"to_id\":\"%s\","
             "\"content\":\"%s\",\"timestamp\":%llu}",
             from_id, to_id, escaped, (unsigned long long)timestamp);

    free(escaped);
    return result;
}

char* protocol_create_system(const char* message)
{
    if (!message) return NULL;
    size_t len = strlen(message) + 128;
    char* result = (char*)malloc(len);
    if (!result) return NULL;
    snprintf(result, len,
             "{\"type\":\"system\",\"message\":\"%s\",\"timestamp\":%lld}",
             message, (long long)time(NULL));
    return result;
}
