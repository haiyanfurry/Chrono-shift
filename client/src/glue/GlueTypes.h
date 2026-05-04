#pragma once
/**
 * GlueTypes.h — 胶水层共享类型定义
 *
 * 所有传输层、UI层、语言间互操作的统一数据结构。
 * 序列化为 JSON 以支持 Java/JNI、Rust/FFI、CLI/GUI 互通。
 */
#include <string>
#include <vector>
#include <cstdint>
#include <ctime>

namespace chrono { namespace glue {

// ============================================================
// 传输类型
// ============================================================
enum class TransportKind : uint8_t {
    Tor,    // .onion
    I2P,    // .b32.i2p
    Local,  // 本地模拟
};

// ============================================================
// 身份
// ============================================================
struct Identity {
    std::string uid;           // 用户可见名称
    std::string address;        // 传输地址 (.onion 或 .b32.i2p)
    TransportKind transport;    // 地址所属传输层
    std::string pubkey;         // 公钥 (Base64)
    uint64_t created = 0;
};

// ============================================================
// 消息
// ============================================================
struct Envelope {
    std::string id;             // 消息唯一ID (UUID)
    std::string from_uid;
    std::string to_uid;
    std::string from_addr;      // 发送方传输地址
    std::string to_addr;        // 接收方传输地址
    TransportKind via;          // 使用的传输层
    std::string text;           // 消息体 (UTF-8)
    uint64_t timestamp = 0;
    bool delivered = false;

    // 序列化 (JSON)
    std::string to_json() const;
    static Envelope from_json(const std::string& json);
};

// ============================================================
// 传输状态
// ============================================================
struct TransportState {
    TransportKind kind;
    bool connected = false;
    bool bootstrapped = false;
    int bootstrap_pct = 0;
    std::string address;        // 我们在此传输层的地址
    int peers = 0;              // 已知节点数
    int circuits = 0;           // 活跃电路/隧道
    uint64_t bytes_up = 0;
    uint64_t bytes_down = 0;
    int uptime_sec = 0;
};

// ============================================================
// 好友请求
// ============================================================
struct HandshakeRequest {
    std::string from_uid;
    std::string from_addr;
    TransportKind via;
    std::string greeting;
    uint64_t timestamp = 0;
};

// ============================================================
// UTF-8/16 安全转换 (Rust FFI 验证)
// ============================================================
struct SafeString {
    std::string utf8;           // 权威存储 (UTF-8)

    // 转为 UTF-16 (Java JNI 用), 确保无截断
    std::vector<char16_t> to_utf16() const;

    // 从 UTF-16 安全转换, 检测截断
    static SafeString from_utf16(const char16_t* data, size_t len);

    // 从 UTF-8 创建, 验证合法性
    static SafeString from_utf8(const std::string& s);
};

} } // namespace chrono::glue
