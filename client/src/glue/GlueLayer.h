#pragma once
/**
 * GlueLayer.h — 胶水层门面
 *
 * 统一入口, CLI 和未来 GUI 都通过此层访问所有功能。
 * 内部协调 TransportInterface + MessageRouter + SocialManager。
 *
 * 架构:
 *   CLI / GUI / API
 *        ↓
 *   GlueLayer (本文件)        ← 胶水层门面
 *        ↓
 *   ┌─────┼─────┐
 *   │     │     │
 *   Transport  MessageRouter  SocialManager
 *   (Tor/I2P)  (跨传输路由)   (好友/消息)
 *
 * Java 胶水: 见 glue/java/JavaBridge.h
 * Rust 胶水: 见 glue/rust/RustFFI.h
 */
#include "glue/GlueTypes.h"
#include "glue/TransportInterface.h"
#include "glue/MessageRouter.h"
#include "glue/IdentityManager.h"
#include <string>
#include <functional>

namespace chrono { namespace glue {

class GlueLayer {
public:
    static GlueLayer& instance();

    // === 初始化 ===
    bool init(const std::string& data_dir = "./data");

    // === 传输 ===
    bool start_transport(TransportKind kind);
    void stop_transport(TransportKind kind);
    TransportState get_transport_state(TransportKind kind) const;
    std::vector<TransportState> all_transport_states() const;

    // === 身份 ===
    void set_uid(const std::string& uid);
    std::string get_uid() const;
    std::string get_address() const;
    TransportKind get_active_transport() const;

    // === 社交 ===
    bool send_friend_request(const std::string& to_uid);
    std::vector<HandshakeRequest> pending_requests() const;
    bool accept_friend(const std::string& uid);
    bool reject_friend(const std::string& uid);
    std::vector<std::string> friend_list() const;

    // === 消息 ===
    bool send_message(const std::string& to_uid, const std::string& text);
    std::vector<Envelope> get_messages(const std::string& with_uid = "");

    // === 事件回调 (GUI 用) ===
    using EventCallback = std::function<void(const std::string& event_type,
                                              const std::string& json_data)>;
    void on_event(EventCallback cb);

    // === 保存/加载 ===
    bool save_state();
    bool load_state();

private:
    GlueLayer() = default;
    std::string data_dir_;
    TransportKind active_transport_ = TransportKind::Local;
    EventCallback event_cb_;
    bool initialized_ = false;
};

} } // namespace chrono::glue
