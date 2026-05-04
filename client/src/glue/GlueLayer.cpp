/**
 * GlueLayer.cpp — 胶水层门面实现
 */
#include "glue/GlueLayer.h"
#include "social/SocialManager.h"
#include <cstdio>

namespace chrono { namespace glue {

GlueLayer& GlueLayer::instance()
{
    static GlueLayer layer;
    return layer;
}

bool GlueLayer::init(const std::string& data_dir)
{
    if (initialized_) return true;
    data_dir_ = data_dir;
    auto& mgr = chrono::client::social::SocialManager::instance();
    mgr.load_state(data_dir);
    initialized_ = true;
    return true;
}

// === 传输 ===

bool GlueLayer::start_transport(TransportKind kind)
{
    active_transport_ = kind;
    if (event_cb_)
        event_cb_("transport_started",
                  "{\"kind\":" + std::to_string((int)kind) + "}");
    return true;
}

void GlueLayer::stop_transport(TransportKind kind)
{
    if (event_cb_)
        event_cb_("transport_stopped",
                  "{\"kind\":" + std::to_string((int)kind) + "}");
}

TransportState GlueLayer::get_transport_state(TransportKind kind) const
{
    TransportState s;
    s.kind = kind;
    return s;
}

std::vector<TransportState> GlueLayer::all_transport_states() const
{
    return {};
}

// === 身份 ===

void GlueLayer::set_uid(const std::string& uid)
{
    chrono::client::social::SocialManager::instance().set_uid(uid);
    if (event_cb_)
        event_cb_("uid_changed", "{\"uid\":\"" + uid + "\"}");
}

std::string GlueLayer::get_uid() const
{
    return chrono::client::social::SocialManager::instance().my_uid();
}

std::string GlueLayer::get_address() const
{
    return chrono::client::social::SocialManager::instance().my_i2p();
}

TransportKind GlueLayer::get_active_transport() const
{
    return active_transport_;
}

// === 社交 ===

bool GlueLayer::send_friend_request(const std::string& to_uid)
{
    auto& mgr = chrono::client::social::SocialManager::instance();
    if (mgr.is_friend(to_uid)) return false;
    mgr.add_pending_request(to_uid, to_uid + ".addr",
                            "Hi! I'm " + mgr.my_uid());
    if (event_cb_)
        event_cb_("friend_request_sent", "{\"to\":\"" + to_uid + "\"}");
    return true;
}

std::vector<HandshakeRequest> GlueLayer::pending_requests() const
{
    std::vector<HandshakeRequest> result;
    auto& mgr = chrono::client::social::SocialManager::instance();
    for (auto& r : mgr.pending_requests()) {
        HandshakeRequest h;
        h.from_uid = r.from_uid;
        h.from_addr = r.from_i2p;
        h.greeting = r.greeting;
        h.timestamp = r.timestamp;
        result.push_back(h);
    }
    return result;
}

bool GlueLayer::accept_friend(const std::string& uid)
{
    auto& mgr = chrono::client::social::SocialManager::instance();
    if (!mgr.has_pending_from(uid)) return false;
    mgr.accept_request(uid);
    if (event_cb_)
        event_cb_("friend_accepted", "{\"uid\":\"" + uid + "\"}");
    return true;
}

bool GlueLayer::reject_friend(const std::string& uid)
{
    auto& mgr = chrono::client::social::SocialManager::instance();
    if (!mgr.has_pending_from(uid)) return false;
    mgr.reject_request(uid);
    return true;
}

std::vector<std::string> GlueLayer::friend_list() const
{
    return chrono::client::social::SocialManager::instance().friend_list();
}

// === 消息 ===

bool GlueLayer::send_message(const std::string& to_uid, const std::string& text)
{
    auto& mgr = chrono::client::social::SocialManager::instance();
    mgr.add_message(mgr.my_uid(), to_uid, text);
    if (event_cb_)
        event_cb_("message_sent",
                  "{\"to\":\"" + to_uid + "\",\"text\":\"" + text + "\"}");
    return true;
}

std::vector<Envelope> GlueLayer::get_messages(const std::string& with_uid)
{
    std::vector<Envelope> result;
    auto& mgr = chrono::client::social::SocialManager::instance();
    for (auto& cm : mgr.get_chat_history(with_uid)) {
        Envelope e;
        e.from_uid = cm.from_uid;
        e.to_uid = cm.to_uid;
        e.text = cm.text;
        e.timestamp = cm.timestamp;
        result.push_back(e);
    }
    return result;
}

// === 事件回调 ===

void GlueLayer::on_event(EventCallback cb)
{
    event_cb_ = cb;
}

// === 持久化 ===

bool GlueLayer::save_state()
{
    return chrono::client::social::SocialManager::instance().save_state(data_dir_);
}

bool GlueLayer::load_state()
{
    return chrono::client::social::SocialManager::instance().load_state(data_dir_);
}

} } // namespace
