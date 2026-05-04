#pragma once
/**
 * ServiceLayer.h — RetroShare p3 风格服务抽象层
 *
 * 借鉴 RetroShare 的 p3 服务模式:
 *   每个服务继承 RsService, 实现 tick() / recv() / info()
 *   全局指针模式: extern ServiceType *g_service;
 *
 * Chrono-shift 服务:
 *   SVC_CHAT       (0x01)  — 即时消息
 *   SVC_FRIEND     (0x02)  — 好友管理 (信任网)
 *   SVC_DISCOVERY  (0x03)  — 节点发现 (DHT)
 *   SVC_FILE       (0x04)  — 文件传输 (预留)
 *   SVC_FORUM      (0x05)  — 论坛/频道 (预留)
 *   SVC_BRIDGE     (0xF0)  — 跨传输桥接控制
 */
#include "glue/GlueTypes.h"
#include <cstdint>
#include <string>
#include <functional>
#include <memory>
#include <map>

namespace chrono { namespace glue {

// === 服务信息 (类似 RetroShare RsServiceInfo) ===
struct ServiceInfo {
    uint16_t id;
    std::string name;
    std::string version;
    uint8_t auth_flags = 0;  // 0=public, 1=friends-only, 2=circle
};

// === 基础服务项 (类似 RetroShare RsItem) ===
struct ServiceItem {
    uint16_t svc_id = 0;
    uint8_t  sub_type = 0;
    std::string from_uid;
    std::string to_uid;
    std::string payload;     // 序列化的服务数据

    virtual ~ServiceItem() = default;
    virtual size_t serial_size() const { return payload.size(); }
    virtual bool serialize(std::string& out) const { out = payload; return true; }
    virtual bool deserialize(const std::string& in) { payload = in; return true; }
};

// === 服务基类 (类似 RetroShare RsPQIService) ===
class ServiceBase {
public:
    explicit ServiceBase(const ServiceInfo& info) : info_(info) {}
    virtual ~ServiceBase() = default;

    const ServiceInfo& info() const { return info_; }

    // tick() — 主循环每次调用, 处理后台工作
    virtual void tick() = 0;

    // 接收消息项
    virtual bool recv_item(std::unique_ptr<ServiceItem> item) = 0;

    // 发送消息项到目标 UID
    virtual bool send_item(const std::string& to_uid,
                           std::unique_ptr<ServiceItem> item) = 0;

    // 事件回调 (新消息/状态变更)
    using EventFn = std::function<void(const std::string& event,
                                        const std::string& data)>;
    virtual void on_event(EventFn fn) { event_fn_ = fn; }

protected:
    ServiceInfo info_;
    EventFn event_fn_;
};

// === 聊天服务 (SVC_CHAT) ===
class ChatService : public ServiceBase {
public:
    ChatService();
    void tick() override;
    bool recv_item(std::unique_ptr<ServiceItem> item) override;
    bool send_item(const std::string& to_uid,
                   std::unique_ptr<ServiceItem> item) override;

    // 高级 API
    bool send_message(const std::string& to, const std::string& text);
    size_t unread_count(const std::string& from_uid = "") const;
};

// === 好友服务 (SVC_FRIEND) — RetroShare F2F 信任网 ===
class FriendService : public ServiceBase {
public:
    FriendService();
    void tick() override;
    bool recv_item(std::unique_ptr<ServiceItem> item) override;
    bool send_item(const std::string& to_uid,
                   std::unique_ptr<ServiceItem> item) override;

    // 信任网
    enum TrustLevel { UNVERIFIED = 0, VERIFIED = 1, FULL_TRUST = 2 };

    bool send_friend_request(const std::string& to_uid,
                             const std::string& greeting);
    bool accept_friend(const std::string& uid, TrustLevel trust = VERIFIED);
    bool reject_friend(const std::string& uid);
    TrustLevel get_trust(const std::string& uid) const;
    std::vector<std::string> friends_at_level(TrustLevel min) const;

private:
    struct FriendEntry {
        std::string uid;
        TrustLevel trust = UNVERIFIED;
        std::string gpg_fingerprint;  // RetroShare 风格: GPG 指纹
        uint64_t added = 0;
    };
    std::map<std::string, FriendEntry> friends_;
};

// === 服务管理器 (类似 RetroShare p3ServiceControl) ===
class ServiceManager {
public:
    static ServiceManager& instance();

    void register_service(std::unique_ptr<ServiceBase> svc);
    ServiceBase* get_service(uint16_t id);

    // 主 tick — 驱动所有服务
    void tick_all();

    // 消息分发 — 根据 svc_id 路由到对应服务
    bool dispatch(std::unique_ptr<ServiceItem> item);

    // 全局指针模式 (RetroShare 风格)
    ChatService* chat() { return chat_; }
    FriendService* friends() { return friends_; }

private:
    ServiceManager() = default;
    std::map<uint16_t, std::unique_ptr<ServiceBase>> services_;
    ChatService* chat_ = nullptr;
    FriendService* friends_ = nullptr;
};

} } // namespace
