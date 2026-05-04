/**
 * ServiceLayer.cpp — RetroShare p3 风格服务实现
 */
#include "glue/ServiceLayer.h"
#include "social/SocialManager.h"
#include <cstdio>

namespace chrono { namespace glue {

// ============================================================
// ChatService
// ============================================================

ChatService::ChatService()
    : ServiceBase({0x01, "SVC_CHAT", "1.0", 1})
{}

void ChatService::tick()
{
    // 检查待发送队列, 处理超时消息
}

bool ChatService::recv_item(std::unique_ptr<ServiceItem> item)
{
    if (!item || item->svc_id != 0x01) return false;
    auto& mgr = chrono::client::social::SocialManager::instance();
    mgr.add_message(item->from_uid, item->to_uid, item->payload);
    if (event_fn_)
        event_fn_("message_received",
                  "{\"from\":\"" + item->from_uid + "\"}");
    return true;
}

bool ChatService::send_item(const std::string& to_uid,
                             std::unique_ptr<ServiceItem> item)
{
    return send_message(to_uid, item->payload);
}

bool ChatService::send_message(const std::string& to, const std::string& text)
{
    auto& mgr = chrono::client::social::SocialManager::instance();
    mgr.add_message(mgr.my_uid(), to, text);
    return true;
}

size_t ChatService::unread_count(const std::string& from_uid) const
{
    auto& mgr = chrono::client::social::SocialManager::instance();
    auto msgs = mgr.get_chat_history(from_uid);
    size_t count = 0;
    for (auto& m : msgs) {
        if (!m.delivered) count++;
    }
    return count;
}

// ============================================================
// FriendService (RetroShare F2F 信任网)
// ============================================================

FriendService::FriendService()
    : ServiceBase({0x02, "SVC_FRIEND", "1.0", 1})
{}

void FriendService::tick()
{
    // 清理过期请求, 更新信任网状态
}

bool FriendService::recv_item(std::unique_ptr<ServiceItem> item)
{
    if (!item || item->svc_id != 0x02) return false;

    // 根据 sub_type 处理不同类型的好友消息
    switch (item->sub_type) {
    case 0x01: // Friend Request
        {
            auto& mgr = chrono::client::social::SocialManager::instance();
            mgr.add_pending_request(item->from_uid, "", item->payload);
        }
        break;
    case 0x02: // Friend Accept
        accept_friend(item->from_uid, VERIFIED);
        break;
    case 0x03: // Trust Update
        friends_[item->from_uid].trust = FULL_TRUST;
        break;
    }
    return true;
}

bool FriendService::send_item(const std::string& to_uid,
                               std::unique_ptr<ServiceItem> item)
{
    (void)to_uid;
    (void)item;
    return true;
}

bool FriendService::send_friend_request(const std::string& to_uid,
                                         const std::string& greeting)
{
    auto& mgr = chrono::client::social::SocialManager::instance();
    mgr.add_pending_request(to_uid, "", greeting);
    return true;
}

bool FriendService::accept_friend(const std::string& uid, TrustLevel trust)
{
    auto& mgr = chrono::client::social::SocialManager::instance();
    mgr.accept_request(uid);
    friends_[uid] = {uid, trust, "", (uint64_t)time(nullptr)};
    if (event_fn_)
        event_fn_("friend_accepted",
                  "{\"uid\":\"" + uid + "\",\"trust\":" + std::to_string(trust) + "}");
    return true;
}

bool FriendService::reject_friend(const std::string& uid)
{
    auto& mgr = chrono::client::social::SocialManager::instance();
    mgr.reject_request(uid);
    return true;
}

FriendService::TrustLevel FriendService::get_trust(const std::string& uid) const
{
    auto it = friends_.find(uid);
    return it != friends_.end() ? it->second.trust : UNVERIFIED;
}

std::vector<std::string> FriendService::friends_at_level(TrustLevel min) const
{
    std::vector<std::string> result;
    for (auto& [uid, entry] : friends_) {
        if (entry.trust >= min) result.push_back(uid);
    }
    return result;
}

// ============================================================
// ServiceManager
// ============================================================

ServiceManager& ServiceManager::instance()
{
    static ServiceManager mgr;
    return mgr;
}

void ServiceManager::register_service(std::unique_ptr<ServiceBase> svc)
{
    if (!svc) return;
    uint16_t id = svc->info().id;
    if (id == 0x01) chat_ = dynamic_cast<ChatService*>(svc.get());
    if (id == 0x02) friends_ = dynamic_cast<FriendService*>(svc.get());
    services_[id] = std::move(svc);
}

ServiceBase* ServiceManager::get_service(uint16_t id)
{
    auto it = services_.find(id);
    return it != services_.end() ? it->second.get() : nullptr;
}

void ServiceManager::tick_all()
{
    for (auto& [id, svc] : services_) {
        svc->tick();
    }
}

bool ServiceManager::dispatch(std::unique_ptr<ServiceItem> item)
{
    if (!item) return false;
    auto* svc = get_service(item->svc_id);
    if (!svc) return false;
    return svc->recv_item(std::move(item));
}

} } // namespace
