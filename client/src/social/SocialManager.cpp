/**
 * SocialManager.cpp — 社交功能管理器实现
 */
#include "social/SocialManager.h"
#include <cstdio>
#include <cstring>
#include <algorithm>

namespace chrono { namespace client { namespace social {

SocialManager& SocialManager::instance()
{
    static SocialManager mgr;
    return mgr;
}

// --- 好友请求 ---

void SocialManager::add_pending_request(const std::string& from_uid,
                                         const std::string& from_i2p,
                                         const std::string& greeting)
{
    FriendRequest req;
    req.from_uid = from_uid;
    req.from_i2p = from_i2p;
    req.greeting = greeting;
    req.timestamp = (uint64_t)std::time(nullptr);
    pending_.push_back(req);
}

std::vector<FriendRequest> SocialManager::pending_requests() const
{
    return pending_;
}

bool SocialManager::has_pending_from(const std::string& uid) const
{
    return std::any_of(pending_.begin(), pending_.end(),
        [&](const FriendRequest& r) { return r.from_uid == uid; });
}

void SocialManager::accept_request(const std::string& from_uid)
{
    for (auto it = pending_.begin(); it != pending_.end(); ++it) {
        if (it->from_uid == from_uid) {
            add_friend(from_uid, it->from_i2p);
            pending_.erase(it);
            return;
        }
    }
}

void SocialManager::reject_request(const std::string& from_uid)
{
    for (auto it = pending_.begin(); it != pending_.end(); ++it) {
        if (it->from_uid == from_uid) {
            block_for_30min(from_uid);
            pending_.erase(it);
            return;
        }
    }
}

// --- 屏蔽 ---

bool SocialManager::is_blocked(const std::string& uid) const
{
    uint64_t now = (uint64_t)std::time(nullptr);
    for (auto& b : blocks_) {
        if (b.blocked_uid == uid && now < b.blocked_until) return true;
    }
    return false;
}

void SocialManager::block_for_30min(const std::string& uid)
{
    cleanup_expired_blocks();
    BlockEntry b;
    b.blocked_uid = uid;
    b.blocked_until = (uint64_t)std::time(nullptr) + 1800;  // 30分钟
    blocks_.push_back(b);
}

void SocialManager::cleanup_expired_blocks()
{
    uint64_t now = (uint64_t)std::time(nullptr);
    blocks_.erase(
        std::remove_if(blocks_.begin(), blocks_.end(),
            [now](const BlockEntry& b) { return now >= b.blocked_until; }),
        blocks_.end());
}

// --- 好友列表 ---

bool SocialManager::is_friend(const std::string& uid) const
{
    return friends_.find(uid) != friends_.end();
}

void SocialManager::add_friend(const std::string& uid, const std::string& i2p)
{
    friends_[uid] = i2p;
}

std::vector<std::string> SocialManager::friend_list() const
{
    std::vector<std::string> result;
    for (auto& [uid, _] : friends_) result.push_back(uid);
    return result;
}

std::string SocialManager::friend_i2p(const std::string& uid) const
{
    auto it = friends_.find(uid);
    return it != friends_.end() ? it->second : "";
}

// --- 消息 ---

void SocialManager::add_message(const std::string& from, const std::string& to,
                                 const std::string& text)
{
    ChatMessage msg;
    msg.from_uid = from;
    msg.to_uid = to;
    msg.text = text;
    msg.timestamp = (uint64_t)std::time(nullptr);
    messages_.push_back(msg);
    if (messages_.size() > 1000) messages_.pop_front();
}

std::vector<ChatMessage> SocialManager::get_chat_history(
    const std::string& with_uid) const
{
    std::vector<ChatMessage> result;
    for (auto& m : messages_) {
        if (with_uid.empty() ||
            (m.from_uid == with_uid && m.to_uid == my_uid_) ||
            (m.to_uid == with_uid && m.from_uid == my_uid_)) {
            result.push_back(m);
        }
    }
    return result;
}

// --- 持久化 ---

bool SocialManager::save_state(const std::string& data_dir)
{
    std::string path = data_dir + "/social_state.json";
    FILE* f = fopen(path.c_str(), "w");
    if (!f) return false;

    fprintf(f, "{\"uid\":\"%s\",\"i2p\":\"%s\",\"friends\":[",
            my_uid_.c_str(), my_i2p_.c_str());
    bool first = true;
    for (auto& [uid, i2p] : friends_) {
        if (!first) fprintf(f, ",");
        fprintf(f, "{\"uid\":\"%s\",\"i2p\":\"%s\"}", uid.c_str(), i2p.c_str());
        first = false;
    }
    fprintf(f, "],\"messages\":[");
    first = true;
    for (auto& m : messages_) {
        if (!first) fprintf(f, ",");
        fprintf(f, "{\"from\":\"%s\",\"to\":\"%s\",\"text\":\"%s\",\"ts\":%llu}",
                m.from_uid.c_str(), m.to_uid.c_str(),
                m.text.c_str(), (unsigned long long)m.timestamp);
        first = false;
    }
    fprintf(f, "]}\n");
    fclose(f);
    return true;
}

bool SocialManager::load_state(const std::string& data_dir)
{
    std::string path = data_dir + "/social_state.json";
    FILE* f = fopen(path.c_str(), "r");
    if (!f) return false;

    char buf[4096];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    if (n == 0) return false;
    buf[n] = '\0';

    // 简易 JSON 解析 (生产用应使用 json_parser)
    const char* p = strstr(buf, "\"uid\":\"");
    if (p) {
        p += 7;
        const char* end = strchr(p, '"');
        if (end) my_uid_ = std::string(p, end - p);
    }
    p = strstr(buf, "\"i2p\":\"");
    if (p) {
        p += 7;
        const char* end = strchr(p, '"');
        if (end) my_i2p_ = std::string(p, end - p);
    }
    return true;
}

} } } // namespace
