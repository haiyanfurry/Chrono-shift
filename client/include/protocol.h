#ifndef CHRONO_PROTOCOL_H
#define CHRONO_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

/* ============================================================
 * 通信协议定义
 * Chrono-shift 自定义应用层协议
 * ============================================================ */

/* --- 消息类型 --- */
enum MessageType {
    MSG_TYPE_HEARTBEAT    = 0x01,
    MSG_TYPE_AUTH         = 0x02,
    MSG_TYPE_AUTH_RESP    = 0x03,
    MSG_TYPE_TEXT         = 0x10,
    MSG_TYPE_IMAGE        = 0x11,
    MSG_TYPE_FILE         = 0x12,
    MSG_TYPE_TYPING       = 0x20,
    MSG_TYPE_READ_RECEIPT = 0x21,
    MSG_TYPE_FRIEND_REQ   = 0x30,
    MSG_TYPE_FRIEND_RESP  = 0x31,
    MSG_TYPE_HANDSHAKE_REQ   = 0x40,  /* I2P 好友请求 */
    MSG_TYPE_HANDSHAKE_ACCEPT = 0x41,  /* 接受好友请求 */
    MSG_TYPE_HANDSHAKE_REJECT = 0x42,  /* 拒绝 (30分钟屏蔽) */
    MSG_TYPE_I2P_IDENTIFY     = 0x43,  /* 交换 I2P 地址 */
    MSG_TYPE_SYSTEM       = 0xFF
};

/* --- 协议头部 (8 字节) --- */
#if defined(_MSC_VER)
#pragma pack(push, 1)
typedef struct {
    uint32_t magic;          /* 魔数: 0x43485346 ('CHSF') */
    uint8_t  version;        /* 协议版本: 0x01 */
    uint8_t  msg_type;       /* 消息类型 */
    uint16_t body_length;    /* 消息体长度（网络字节序） */
} ProtocolHeader;
#pragma pack(pop)
#else
typedef struct __attribute__((packed)) {
    uint32_t magic;          /* 魔数: 0x43485346 ('CHSF') */
    uint8_t  version;        /* 协议版本: 0x01 */
    uint8_t  msg_type;       /* 消息类型 */
    uint16_t body_length;    /* 消息体长度（网络字节序） */
} ProtocolHeader;
#endif

#define PROTOCOL_MAGIC      0x43485346
#define PROTOCOL_VERSION    0x01
#define HEADER_SIZE         sizeof(ProtocolHeader)
#define MAX_BODY_SIZE       65535
#define MAX_PACKET_SIZE     (HEADER_SIZE + MAX_BODY_SIZE)

/* --- JSON 协议字段常量 --- */
#define PROTOCOL_KEY_TYPE       "type"
#define PROTOCOL_KEY_DATA       "data"
#define PROTOCOL_KEY_STATUS     "status"
#define PROTOCOL_KEY_MESSAGE    "message"
#define PROTOCOL_KEY_USER_ID    "user_id"
#define PROTOCOL_KEY_USERNAME   "username"
#define PROTOCOL_KEY_TOKEN      "token"
#define PROTOCOL_KEY_CONTENT    "content"
#define PROTOCOL_KEY_TIMESTAMP  "timestamp"
#define PROTOCOL_KEY_FROM_ID    "from_id"
#define PROTOCOL_KEY_TO_ID      "to_id"
#define PROTOCOL_KEY_CHAT_ID    "chat_id"

/* --- 头部编解码 --- */
uint16_t protocol_encode_header(uint8_t* buffer, size_t buf_size,
                                uint8_t msg_type, const uint8_t* body, uint16_t body_len);
int      protocol_decode_header(const uint8_t* buffer, size_t buf_size,
                                ProtocolHeader* header);

/* --- 完整包编解码 --- */
/* 编码: 写入 header + body，返回总字节数 */
uint16_t protocol_encode_packet(uint8_t* buffer, size_t buf_size,
                                uint8_t msg_type, const uint8_t* body, uint16_t body_len);

/* 解码: 从字节流中提取一个完整包，返回包总大小(header+body)，-1 表示数据不足或无效 */
int      protocol_decode_packet(const uint8_t* buffer, size_t buf_size,
                                ProtocolHeader* header, const uint8_t** body_ptr);

/* --- 辅助函数：构建消息体 (JSON) --- */
/* 调用者需 free 返回的字符串 */
char*    protocol_create_heartbeat(void);
char*    protocol_create_auth(const char* token);
char*    protocol_create_text(const char* from_id, const char* to_id,
                              const char* content, uint64_t timestamp);
char*    protocol_create_system(const char* message);

#endif /* CHRONO_PROTOCOL_H */
