/**
 * Chrono-shift C++ 数据库层
 * 文件系统数据库，封装 db_core 操作
 * C++17 重构版
 */
#ifndef CHRONO_CPP_DATABASE_H
#define CHRONO_CPP_DATABASE_H

#include "../json/JsonValue.h"
#include <string>
#include <vector>
#include <optional>
#include <memory>

namespace chrono {
namespace db {

// 用户数据结构
struct UserData {
    std::string user_id;
    std::string username;
    std::string nickname;
    std::string password_hash;
    std::string email;
    std::string avatar_path;
    std::string created_at;
    std::string bio;
};

// 消息数据结构
struct MessageData {
    std::string message_id;
    std::string from_id;
    std::string to_id;
    std::string content;
    uint64_t timestamp;
    int msg_type; // 0=text, 1=image, 2=file
};

// 好友关系
struct FriendshipData {
    std::string user_id;
    std::string friend_id;
    std::string status; // "pending", "accepted", "blocked"
    uint64_t created_at;
};

// 模板数据
struct TemplateData {
    std::string template_id;
    std::string name;
    std::string content;
    std::string author_id;
    uint64_t created_at;
};

/**
 * 数据库管理器 — RAII 封装 db_core 文件操作
 */
class Database {
public:
    explicit Database(const std::string& data_path);
    ~Database() = default;

    // 禁止拷贝
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    /**
     * 初始化数据库路径
     */
    bool init();

    // ============================================================
    // 用户操作
    // ============================================================
    std::optional<UserData> get_user(const std::string& user_id);
    std::optional<UserData> get_user_by_username(const std::string& username);
    std::optional<UserData> get_user_by_email(const std::string& email);
    bool save_user(const UserData& user);
    bool update_user(const UserData& user);
    bool delete_user(const std::string& user_id);
    std::vector<UserData> search_users(const std::string& keyword, int limit = 20);
    std::string generate_user_id();

    // ============================================================
    // 消息操作
    // ============================================================
    std::optional<MessageData> get_message(const std::string& message_id);
    bool save_message(const MessageData& msg);
    std::vector<MessageData> get_messages(const std::string& user_id,
                                           const std::string& other_id,
                                           int limit = 50, int offset = 0);

    // ============================================================
    // 好友操作
    // ============================================================
    bool add_friend(const std::string& user_id, const std::string& friend_id);
    bool remove_friend(const std::string& user_id, const std::string& friend_id);
    bool accept_friend(const std::string& user_id, const std::string& friend_id);
    std::vector<FriendshipData> get_friends(const std::string& user_id);
    bool is_friend(const std::string& user_id, const std::string& friend_id);

    // ============================================================
    // 模板操作
    // ============================================================
    bool save_template(const TemplateData& tmpl);
    std::optional<TemplateData> get_template(const std::string& template_id);
    std::vector<TemplateData> list_templates(int limit = 50, int offset = 0);

    // ============================================================
    // OAuth 账号绑定 (P9.2)
    // ============================================================
    bool save_oauth_account(const std::string& platform, const std::string& open_id,
                             const std::string& user_id);
    std::optional<std::string> get_user_by_oauth(const std::string& platform,
                                                   const std::string& open_id);
    bool remove_oauth_account(const std::string& platform, const std::string& open_id);

private:
    std::string data_path_;

    // 内部文件操作
    bool ensure_dir(const std::string& dir);
    std::string read_file(const std::string& path);
    bool write_file(const std::string& path, const std::string& content);
    bool file_exists(const std::string& path);
    std::string read_next_id(const std::string& counter_path);
    bool write_next_id(const std::string& counter_path, const std::string& value);

    // 路径生成
    std::string user_path(const std::string& user_id) const;
    std::string username_index_path() const;
    std::string email_index_path() const;
    std::string message_path(const std::string& msg_id) const;
    std::string friendship_path(const std::string& user_id) const;
    std::string template_path(const std::string& tmpl_id) const;
    std::string oauth_index_path(const std::string& platform) const;
};

} // namespace db
} // namespace chrono

#endif // CHRONO_CPP_DATABASE_H
