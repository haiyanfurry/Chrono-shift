#pragma once
/**
 * TransportInterface.h — 传输层抽象接口
 *
 * 所有传输层 (Tor / I2P / Local) 实现此接口。
 * 胶水层通过此接口统一访问传输层, 不关心底层实现。
 *
 * 后期 GUI 桌面版通过此接口接入, 无需修改传输代码。
 */
#include "glue/GlueTypes.h"
#include <functional>
#include <memory>

namespace chrono { namespace glue {

class TransportInterface {
public:
    virtual ~TransportInterface() = default;

    // === 生命周期 ===
    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual TransportKind kind() const = 0;
    virtual TransportState get_state() const = 0;

    // === 消息收发 ===
    // 发送消息到目标地址 (跨传输路由由 MessageRouter 处理)
    virtual bool send(const std::string& to_addr, const std::string& payload) = 0;

    // 接收回调: (from_addr, payload) → void
    using ReceiveCallback = std::function<void(const std::string&, const std::string&)>;
    virtual void on_receive(ReceiveCallback cb) = 0;

    // === 地址解析 ===
    virtual std::string lookup(const std::string& address) = 0;

    // === 状态回调 ===
    using StateCallback = std::function<void(const TransportState&)>;
    virtual void on_state_change(StateCallback cb) = 0;

    // === 日志 ===
    virtual std::string get_log() const = 0;
};

// 工厂函数
std::unique_ptr<TransportInterface> create_tor_transport();
std::unique_ptr<TransportInterface> create_i2p_transport();
std::unique_ptr<TransportInterface> create_local_transport();

} } // namespace chrono::glue
