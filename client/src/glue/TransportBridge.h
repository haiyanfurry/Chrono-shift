#pragma once
/**
 * TransportBridge.h — 跨传输桥接器 (I2P ↔ Tor)
 *
 * 借鉴 RetroShare 的 F2F 模型:
 *   桥接节点同时运行 Tor 和 I2P, 在两个网络间转发消息。
 *
 * 架构:
 *   I2P Network ←→ Bridge Node ←→ Tor Network
 *                      ↓
 *               chrono-bridge (本文件)
 *
 * 桥接节点拥有双地址:
 *   .b32.i2p address  (I2P 侧)
 *   .onion address    (Tor 侧)
 *
 * 消息路由:
 *   I2P用户 → 桥接.onion → Tor用户
 *   Tor用户 → 桥接.b32.i2p → I2P用户
 */
#include "glue/GlueTypes.h"
#include "glue/TransportInterface.h"
#include <map>
#include <queue>
#include <mutex>
#include <memory>

namespace chrono { namespace glue {

struct BridgeRoute {
    std::string target_uid;
    std::string target_addr;     // 目标在目标网络的地址
    TransportKind src_network;   // 来源网络
    TransportKind dst_network;   // 目标网络
    uint64_t established = 0;
};

class TransportBridge {
public:
    static TransportBridge& instance();

    // === 桥接注册 ===
    // 注册一个传输层用于桥接 (需要同时有 Tor 和 I2P)
    void register_transport(TransportKind kind,
                            std::unique_ptr<TransportInterface> transport);

    // === 双地址 ===
    void set_i2p_address(const std::string& addr) { i2p_addr_ = addr; }
    void set_tor_address(const std::string& addr) { tor_addr_ = addr; }
    std::string i2p_address() const { return i2p_addr_; }
    std::string tor_address() const { return tor_addr_; }

    // === 路由表 ===
    // 注册跨传输路由: UID "alice" 在 Tor 网络, UID "bob" 在 I2P 网络
    void add_route(const std::string& uid,
                   const std::string& tor_addr,
                   const std::string& i2p_addr);

    // 查找到目标 UID 的最佳路由
    BridgeRoute* find_route(const std::string& uid);

    // === 消息桥接 ===
    // 从源网络收到消息, 桥接到目标网络
    bool bridge_message(const Envelope& msg);

    // 队列管理 (RetroShare 风格: tick() 驱动)
    void tick();  // 每次调用处理一个待转发消息
    size_t queue_size() const { return bridge_queue_.size(); }

    // === 发现 ===
    // 向已知桥接节点宣告自己的双地址
    void announce();

private:
    TransportBridge() = default;

    std::string i2p_addr_;
    std::string tor_addr_;
    std::map<TransportKind, std::unique_ptr<TransportInterface>> transports_;
    std::map<std::string, BridgeRoute> routes_;
    std::queue<Envelope> bridge_queue_;
    mutable std::mutex mutex_;
};

} } // namespace
