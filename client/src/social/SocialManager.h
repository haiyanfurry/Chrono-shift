#pragma once
/**
 * SocialManager.h — 社交功能管理器
 *
 * 管理: 身份、好友请求、屏蔽列表、消息收发
 */
#include <string>
#include <vector>
#include <cstdint>
#include <ctime>
#include <map>
#include <deque>

namespace chrono { namespace client { namespace social {

struct FriendRequest {
    std::string from_uid;
    std::string from_i2p;
    std::string greeting;
    uint64_t timestamp;
};

struct BlockEntry {
    std::string blocked_uid;
    uint64_t blocked_until;
};

struct ChatMessage {
    std::string from_uid;
    std::string to_uid;
    std::string text;
    uint64_t timestamp;
    bool delivered = false;
};

class SocialManager {
public:
    static SocialManager& instance();

    // 身份
    void set_uid(const std::string& uid) { my_uid_ = uid; }
    const std::string& my_uid() const { return my_uid_; }
    void set_i2p_addr(const std::string& addr) { my_i2p_ = addr; }
    const std::string& my_i2p() const { return my_i2p_; }

    // 好友请求
    void add_pending_request(const std::string& from_uid,
                             const std::string& from_i2p,
                             const std::string& greeting);
    std::vector<FriendRequest> pending_requests() const;
    bool has_pending_from(const std::string& uid) const;
    void accept_request(const std::string& from_uid);
    void reject_request(const std::string& from_uid);

    // 屏蔽 (30分钟)
    bool is_blocked(const std::string& uid) const;
    void block_for_30min(const std::string& uid);
    void cleanup_expired_blocks();

    // 好友列表
    bool is_friend(const std::string& uid) const;
    void add_friend(const std::string& uid, const std::string& i2p = "");
    std::vector<std::string> friend_list() const;
    std::string friend_i2p(const std::string& uid) const;

    // 消息
    void add_message(const std::string& from, const std::string& to,
                     const std::string& text);
    std::vector<ChatMessage> get_chat_history(const std::string& with_uid) const;

    // 持久化
    bool save_state(const std::string& data_dir);
    bool load_state(const std::string& data_dir);

private:
    SocialManager() = default;
    std::string my_uid_;
    std::string my_i2p_;
    std::vector<FriendRequest> pending_;
    std::vector<BlockEntry> blocks_;
    std::map<std::string, std::string> friends_;  // uid -> i2p_addr
    std::deque<ChatMessage> messages_;
};

} } } // namespace
