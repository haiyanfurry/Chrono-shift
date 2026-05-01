/**
 * Chrono-shift C++ 通信协议编解码实现
 */
#include "Protocol.h"
#include "StringUtils.h"
#include <cstring>
#include <cstdio>
#include <ctime>
#include <sstream>

namespace chrono {
namespace util {

// ============================================================
// 头部编解码
// ============================================================

std::optional<std::vector<uint8_t>> Protocol::encode_header(
    MessageType msg_type, uint16_t body_len) {
    if (body_len > kMaxBodySize) {
        return std::nullopt;
    }

    std::vector<uint8_t> buffer(kHeaderSize);
    ProtocolHeader header;
    header.magic = kProtocolMagic;
    header.version = kProtocolVersion;
    header.msg_type = static_cast<uint8_t>(msg_type);
    header.body_length = body_len;

    std::memcpy(buffer.data(), &header, kHeaderSize);
    return buffer;
}

std::optional<ProtocolHeader> Protocol::decode_header(
    const uint8_t* buffer, size_t buf_size) {
    if (buf_size < kHeaderSize || !buffer) {
        return std::nullopt;
    }

    ProtocolHeader header;
    std::memcpy(&header, buffer, kHeaderSize);

    // 验证魔数
    if (header.magic != kProtocolMagic) {
        return std::nullopt;
    }

    // 验证版本
    if (header.version != kProtocolVersion) {
        return std::nullopt;
    }

    return header;
}

// ============================================================
// 完整包编解码
// ============================================================

std::optional<std::vector<uint8_t>> Protocol::encode_packet(
    MessageType msg_type, const std::vector<uint8_t>& body) {
    if (body.size() > kMaxBodySize) {
        return std::nullopt;
    }

    uint16_t body_len = static_cast<uint16_t>(body.size());
    std::vector<uint8_t> packet(kHeaderSize + body_len);

    // 编码头部
    ProtocolHeader header;
    header.magic = kProtocolMagic;
    header.version = kProtocolVersion;
    header.msg_type = static_cast<uint8_t>(msg_type);
    header.body_length = body_len;

    std::memcpy(packet.data(), &header, kHeaderSize);

    // 复制 body
    if (body_len > 0) {
        std::memcpy(packet.data() + kHeaderSize, body.data(), body_len);
    }

    return packet;
}

std::optional<std::vector<uint8_t>> Protocol::encode_packet(
    MessageType msg_type, const std::string& body_json) {
    std::vector<uint8_t> body(body_json.begin(), body_json.end());
    return encode_packet(msg_type, body);
}

std::optional<Protocol::Packet> Protocol::decode_packet(
    const uint8_t* buffer, size_t buf_size) {
    if (!buffer || buf_size < kHeaderSize) {
        return std::nullopt;
    }

    // 解码头部
    auto header_opt = decode_header(buffer, buf_size);
    if (!header_opt) {
        return std::nullopt;
    }

    const ProtocolHeader& header = *header_opt;
    uint16_t body_len = header.body_length;

    // 检查 body 是否完整
    if (buf_size < kHeaderSize + body_len) {
        return std::nullopt; // 数据不完整
    }

    Packet packet;
    packet.header = header;
    if (body_len > 0) {
        packet.body.assign(
            buffer + kHeaderSize,
            buffer + kHeaderSize + body_len);
    }

    return packet;
}

// ============================================================
// 消息体构建
// ============================================================

std::string Protocol::create_heartbeat() {
    char buf[64];
    int n = snprintf(buf, sizeof(buf),
                     R"({"type":"heartbeat","timestamp":%lld})",
                     static_cast<long long>(std::time(nullptr)));
    if (n < 0 || static_cast<size_t>(n) >= sizeof(buf)) {
        return R"({"type":"heartbeat","timestamp":0})";
    }
    return std::string(buf);
}

std::string Protocol::create_auth(const std::string& token) {
    char buf[1024];
    int n = snprintf(buf, sizeof(buf),
                     R"({"type":"auth","token":"%s","timestamp":%lld})",
                     token.c_str(),
                     static_cast<long long>(std::time(nullptr)));
    if (n < 0 || static_cast<size_t>(n) >= sizeof(buf)) {
        return R"({"type":"auth","error":"buffer_overflow"})";
    }
    return std::string(buf);
}

std::string Protocol::create_text(const std::string& from_id,
                                   const std::string& to_id,
                                   const std::string& content,
                                   uint64_t timestamp) {
    // 简单的 JSON 转义
    std::string escaped_content;
    escaped_content.reserve(content.length());
    for (char c : content) {
        switch (c) {
            case '"':  escaped_content += "\\\""; break;
            case '\\': escaped_content += "\\\\"; break;
            case '\n': escaped_content += "\\n";  break;
            case '\r': escaped_content += "\\r";  break;
            case '\t': escaped_content += "\\t";  break;
            default:   escaped_content += c;       break;
        }
    }

    char buf[4096];
    int n = snprintf(buf, sizeof(buf),
                     R"({"type":"text","from_id":"%s","to_id":"%s",)"
                     R"("content":"%s","timestamp":%llu})",
                     from_id.c_str(), to_id.c_str(),
                     escaped_content.c_str(),
                     static_cast<unsigned long long>(timestamp));
    if (n < 0 || static_cast<size_t>(n) >= sizeof(buf)) {
        return R"({"type":"text","error":"buffer_overflow"})";
    }
    return std::string(buf);
}

std::string Protocol::create_system(const std::string& message) {
    // 简单的 JSON 转义
    std::string escaped;
    escaped.reserve(message.length());
    for (char c : message) {
        switch (c) {
            case '"':  escaped += "\\\""; break;
            case '\\': escaped += "\\\\"; break;
            case '\n': escaped += "\\n";  break;
            case '\r': escaped += "\\r";  break;
            case '\t': escaped += "\\t";  break;
            default:   escaped += c;       break;
        }
    }

    char buf[2048];
    int n = snprintf(buf, sizeof(buf),
                     R"({"type":"system","message":"%s","timestamp":%lld})",
                     escaped.c_str(),
                     static_cast<long long>(std::time(nullptr)));
    if (n < 0 || static_cast<size_t>(n) >= sizeof(buf)) {
        return R"({"type":"system","error":"buffer_overflow"})";
    }
    return std::string(buf);
}

} // namespace util
} // namespace chrono
