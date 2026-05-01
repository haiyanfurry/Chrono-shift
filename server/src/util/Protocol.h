/**
 * Chrono-shift C++ 通信协议编解码
 * 自定义二进制协议 (8字节头部 + JSON body)
 * C++17 重构版
 */
#ifndef CHRONO_CPP_PROTOCOL_H
#define CHRONO_CPP_PROTOCOL_H

#include <cstdint>
#include <vector>
#include <string>
#include <optional>
#include <array>

namespace chrono {
namespace util {

// 消息类型
enum class MessageType : uint8_t {
    kHeartbeat    = 0x01,
    kAuth         = 0x02,
    kAuthResp     = 0x03,
    kText         = 0x10,
    kImage        = 0x11,
    kFile         = 0x12,
    kTyping       = 0x20,
    kReadReceipt  = 0x21,
    kFriendReq    = 0x30,
    kFriendResp   = 0x31,
    kSystem       = 0xFF
};

// 协议头部 (8 字节, packed)
struct __attribute__((packed)) ProtocolHeader {
    uint32_t magic;         // 魔数: 0x43485346 ('CHSF')
    uint8_t  version;       // 协议版本: 0x01
    uint8_t  msg_type;      // 消息类型
    uint16_t body_length;   // 消息体长度 (网络字节序)
};

// 协议常量
constexpr uint32_t kProtocolMagic   = 0x43485346;
constexpr uint8_t  kProtocolVersion = 0x01;
constexpr size_t   kHeaderSize      = sizeof(ProtocolHeader);
constexpr uint16_t kMaxBodySize     = 65535;
constexpr size_t   kMaxPacketSize   = kHeaderSize + kMaxBodySize;

// JSON 协议字段键名
namespace ProtocolKey {
    constexpr const char* kType      = "type";
    constexpr const char* kData      = "data";
    constexpr const char* kStatus    = "status";
    constexpr const char* kMessage   = "message";
    constexpr const char* kUserId    = "user_id";
    constexpr const char* kUsername  = "username";
    constexpr const char* kToken     = "token";
    constexpr const char* kContent   = "content";
    constexpr const char* kTimestamp = "timestamp";
    constexpr const char* kFromId    = "from_id";
    constexpr const char* kToId      = "to_id";
    constexpr const char* kChatId    = "chat_id";
}

/**
 * 协议编解码器
 * RAII 方式管理编解码操作
 */
class Protocol {
public:
    Protocol() = default;

    // 头部编解码
    static std::optional<std::vector<uint8_t>> encode_header(
        MessageType msg_type, uint16_t body_len);

    static std::optional<ProtocolHeader> decode_header(
        const uint8_t* buffer, size_t buf_size);

    // 完整包编解码
    static std::optional<std::vector<uint8_t>> encode_packet(
        MessageType msg_type, const std::vector<uint8_t>& body);

    static std::optional<std::vector<uint8_t>> encode_packet(
        MessageType msg_type, const std::string& body_json);

    struct Packet {
        ProtocolHeader header;
        std::vector<uint8_t> body;
    };

    static std::optional<Packet> decode_packet(
        const uint8_t* buffer, size_t buf_size);

    // 消息体构建 (返回 JSON 字符串)
    static std::string create_heartbeat();
    static std::string create_auth(const std::string& token);
    static std::string create_text(const std::string& from_id,
                                    const std::string& to_id,
                                    const std::string& content,
                                    uint64_t timestamp);
    static std::string create_system(const std::string& message);
};

} // namespace util
} // namespace chrono

#endif // CHRONO_CPP_PROTOCOL_H
