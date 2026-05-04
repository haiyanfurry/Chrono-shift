/**
 * TransportBridge.cpp — 跨传输桥接实现
 */
#include "glue/TransportBridge.h"
#include <cstdio>

namespace chrono { namespace glue {

TransportBridge& TransportBridge::instance()
{
    static TransportBridge bridge;
    return bridge;
}

void TransportBridge::register_transport(TransportKind kind,
    std::unique_ptr<TransportInterface> transport)
{
    std::lock_guard<std::mutex> lock(mutex_);
    transports_[kind] = std::move(transport);
}

void TransportBridge::add_route(const std::string& uid,
                                 const std::string& tor_addr,
                                 const std::string& i2p_addr)
{
    std::lock_guard<std::mutex> lock(mutex_);
    BridgeRoute route;
    route.target_uid = uid;
    route.target_addr = tor_addr.empty() ? i2p_addr : tor_addr;
    route.src_network = tor_addr.empty() ? TransportKind::I2P : TransportKind::Tor;
    route.dst_network = tor_addr.empty() ? TransportKind::Tor : TransportKind::I2P;
    route.established = (uint64_t)time(nullptr);
    routes_[uid] = route;
}

BridgeRoute* TransportBridge::find_route(const std::string& uid)
{
    auto it = routes_.find(uid);
    return it != routes_.end() ? &it->second : nullptr;
}

bool TransportBridge::bridge_message(const Envelope& msg)
{
    auto* route = find_route(msg.to_uid);
    if (!route) {
        // 无跨传输路由, 放入队列等待
        std::lock_guard<std::mutex> lock(mutex_);
        bridge_queue_.push(msg);
        return false;
    }

    // 找到路由: 通过目标传输层发送
    auto it = transports_.find(route->dst_network);
    if (it == transports_.end()) return false;

    std::string payload = msg.to_json();
    bool ok = it->second->send(route->target_addr, payload);
    return ok;
}

void TransportBridge::tick()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (bridge_queue_.empty()) return;

    auto msg = bridge_queue_.front();
    bridge_queue_.pop();

    // 重试路由
    auto* route = find_route(msg.to_uid);
    if (route) {
        auto it = transports_.find(route->dst_network);
        if (it != transports_.end()) {
            it->second->send(route->target_addr, msg.to_json());
        }
    } else {
        // 仍然无法路由, 放回队列 (最多保留100条)
        if (bridge_queue_.size() < 100)
            bridge_queue_.push(msg);
    }
}

void TransportBridge::announce()
{
    // 向已知桥接节点宣告双地址
    // 格式: {"type":"bridge_announce","i2p":"xxx.b32.i2p","tor":"xxx.onion"}
    std::string announce = "{\"type\":\"bridge_announce\","
                           "\"i2p\":\"" + i2p_addr_ + "\","
                           "\"tor\":\"" + tor_addr_ + "\"}";
    for (auto& [kind, transport] : transports_) {
        if (transport) {
            // 向桥接注册节点发送宣告
        }
    }
}

} } // namespace
