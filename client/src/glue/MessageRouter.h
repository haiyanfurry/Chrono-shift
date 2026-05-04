#pragma once
/**
 * MessageRouter.h — 跨传输消息路由器
 *
 * 核心功能: I2P 用户 ↔ Tor 用户 互通
 *
 * 寻址规则:
 *   .b32.i2p  → I2P transport
 *   .onion     → Tor transport
 *   @uid       → 查 IdentityManager 获取地址和传输层
 *
 * 跨传输时:
 *   发送方传输 → MessageRouter(缓冲+转换) → 目标传输
 *
 * 未来: 可扩展为去中心化 DHT 查找表
 */
#include "glue/GlueTypes.h"
#include "glue/TransportInterface.h"
#include "social/SocialManager.h"
#include <map>
#include <queue>
#include <mutex>
#include <memory>

namespace chrono { namespace glue {

class MessageRouter {
public:
    static MessageRouter& instance();

    // === 传输注册 ===
    void register_transport(TransportKind kind,
                            std::unique_ptr<TransportInterface> transport);
    TransportInterface* get_transport(TransportKind kind);

    // === 消息路由 ===
    // 根据目标地址自动选择传输层发送
    bool route(const Envelope& msg);

    // 接收消息并分发到对应 UID 的 SocialManager
    void on_incoming(const std::string& from_addr,
                     const std::string& payload);

    // === 跨传输桥接 ===
    // 检查是否可以从 src 传输层路由到 dst 传输层
    bool can_route(TransportKind src, TransportKind dst) const;

    // === 缓冲区 (跨传输时暂存) ===
    void buffer_message(const Envelope& msg);
    std::vector<Envelope> drain_buffer(const std::string& for_uid);

    // === 地址簿 ===
    void register_address(const std::string& uid,
                          const std::string& addr,
                          TransportKind kind);
    std::string resolve_uid(const std::string& uid) const;

private:
    MessageRouter() = default;

    std::map<TransportKind, std::unique_ptr<TransportInterface>> transports_;
    std::map<std::string, Identity> address_book_;
    std::queue<Envelope> buffer_;
    mutable std::mutex mutex_;
};

} } // namespace chrono::glue
