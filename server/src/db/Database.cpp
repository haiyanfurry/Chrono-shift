/**
 * Chrono-shift C++ 数据库层实现
 * 文件系统数据库操作
 * C++17 重构版
 */
#include "Database.h"
#include "../json/JsonValue.h"
#include "../json/JsonParser.h"
#include "../util/Logger.h"

#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <ctime>

namespace fs = std::filesystem;

namespace chrono {
namespace db {

// 引入 JSON 命名空间以便使用 json_object() 等便利函数
using namespace chrono::json;

// ============================================================
// 构造函数
// ============================================================
Database::Database(const std::string& data_path)
    : data_path_(data_path)
{
}

// ============================================================
// 初始化
// ============================================================
bool Database::init()
{
    if (!ensure_dir(data_path_)) {
        return false;
    }
    ensure_dir(data_path_ + "/users");
    ensure_dir(data_path_ + "/messages");
    ensure_dir(data_path_ + "/friendships");
    ensure_dir(data_path_ + "/templates");
    ensure_dir(data_path_ + "/oauth");
    LOG_INFO("[DB] Database initialized at %s", data_path_.c_str());
    return true;
}

// ============================================================
// 路径生成
// ============================================================
std::string Database::user_path(const std::string& user_id) const
{
    return data_path_ + "/users/" + user_id + ".json";
}

std::string Database::username_index_path() const
{
    return data_path_ + "/users/username_index.json";
}

std::string Database::email_index_path() const
{
    return data_path_ + "/users/email_index.json";
}

std::string Database::message_path(const std::string& msg_id) const
{
    return data_path_ + "/messages/" + msg_id + ".json";
}

std::string Database::friendship_path(const std::string& user_id) const
{
    return data_path_ + "/friendships/" + user_id + "_friends.json";
}

std::string Database::template_path(const std::string& tmpl_id) const
{
    return data_path_ + "/templates/" + tmpl_id + ".json";
}

std::string Database::oauth_index_path(const std::string& platform) const
{
    return data_path_ + "/oauth/" + platform + "_index.json";
}

// ============================================================
// 文件操作
// ============================================================
bool Database::ensure_dir(const std::string& dir)
{
    std::error_code ec;
    if (fs::exists(dir, ec)) {
        return true;
    }
    return fs::create_directories(dir, ec);
}

std::string Database::read_file(const std::string& path)
{
    std::ifstream ifs(path, std::ios::in | std::ios::binary);
    if (!ifs.is_open()) {
        return "";
    }
    std::ostringstream oss;
    oss << ifs.rdbuf();
    return oss.str();
}

bool Database::write_file(const std::string& path, const std::string& content)
{
    std::ofstream ofs(path, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!ofs.is_open()) {
        LOG_ERROR("[DB] Failed to write file: %s", path.c_str());
        return false;
    }
    ofs.write(content.data(), static_cast<std::streamsize>(content.size()));
    return ofs.good();
}

bool Database::file_exists(const std::string& path)
{
    std::error_code ec;
    return fs::exists(path, ec);
}

std::string Database::read_next_id(const std::string& counter_path)
{
    std::string content = read_file(counter_path);
    if (content.empty()) {
        return "1";
    }
    JsonParser parser;
    auto val = parser.parse(content);
    if (val && val->is_number()) {
        int64_t n = static_cast<int64_t>(val->as_double());
        return std::to_string(n);
    }
    return "1";
}

bool Database::write_next_id(const std::string& counter_path, const std::string& value)
{
    JsonValue obj = json_object();
    obj.object_insert("next_id", JsonValue(std::stoll(value)));
    return write_file(counter_path, obj.serialize());
}

// ============================================================
// 用户操作
// ============================================================
std::string Database::generate_user_id()
{
    std::string counter_path = data_path_ + "/users/counter.json";
    std::string id_str = read_next_id(counter_path);
    int64_t id = std::stoll(id_str);
    write_next_id(counter_path, std::to_string(id + 1));
    char buf[32];
    snprintf(buf, sizeof(buf), "u_%06lld", (long long)id);
    return std::string(buf);
}

std::optional<UserData> Database::get_user(const std::string& user_id)
{
    std::string path = user_path(user_id);
    if (!file_exists(path)) {
        return std::nullopt;
    }
    std::string content = read_file(path);
    JsonParser parser;
    auto json = parser.parse(content);
    if (!json || !json->is_object()) {
        return std::nullopt;
    }

    UserData user;
    user.user_id       = (*json)["user_id"].get_string("");
    user.username      = (*json)["username"].get_string("");
    user.nickname      = (*json)["nickname"].get_string("");
    user.password_hash = (*json)["password_hash"].get_string("");
    user.email         = (*json)["email"].get_string("");
    user.avatar_path   = (*json)["avatar_path"].get_string("");
    user.created_at    = (*json)["created_at"].get_string("");
    user.bio           = (*json)["bio"].get_string("");
    return user;
}

std::optional<UserData> Database::get_user_by_username(const std::string& username)
{
    std::string idx_path = username_index_path();
    if (!file_exists(idx_path)) {
        return std::nullopt;
    }
    std::string idx_content = read_file(idx_path);
    JsonParser parser;
    auto idx_json = parser.parse(idx_content);
    if (!idx_json || !idx_json->is_object()) {
        return std::nullopt;
    }
    auto user_id = (*idx_json)[username].get_string("");
    if (user_id.empty()) {
        return std::nullopt;
    }
    return get_user(user_id);
}

std::optional<UserData> Database::get_user_by_email(const std::string& email)
{
    std::string idx_path = email_index_path();
    if (!file_exists(idx_path)) {
        return std::nullopt;
    }
    std::string idx_content = read_file(idx_path);
    JsonParser parser;
    auto idx_json = parser.parse(idx_content);
    if (!idx_json || !idx_json->is_object()) {
        return std::nullopt;
    }
    auto user_id = (*idx_json)[email].get_string("");
    if (user_id.empty()) {
        return std::nullopt;
    }
    return get_user(user_id);
}

bool Database::save_user(const UserData& user)
{
    // 构建 JSON 对象
    JsonValue obj = json_object();
    obj.object_insert("user_id",       JsonValue(user.user_id));
    obj.object_insert("username",      JsonValue(user.username));
    obj.object_insert("nickname",      JsonValue(user.nickname));
    obj.object_insert("password_hash", JsonValue(user.password_hash));
    obj.object_insert("email",         JsonValue(user.email));
    obj.object_insert("avatar_path",   JsonValue(user.avatar_path));
    obj.object_insert("created_at",    JsonValue(user.created_at));
    obj.object_insert("bio",           JsonValue(user.bio));

    // 写入用户文件
    if (!write_file(user_path(user.user_id), obj.serialize(true))) {
        return false;
    }

    // 更新用户名索引
    {
        std::string idx_path = username_index_path();
        JsonValue idx_obj = json_object();
        if (file_exists(idx_path)) {
            JsonParser parser;
            auto existing = parser.parse(read_file(idx_path));
            if (existing && existing->is_object()) {
                idx_obj = std::move(*existing);
            }
        }
        idx_obj.object_insert(user.username, JsonValue(user.user_id));
        if (!write_file(idx_path, idx_obj.serialize(true))) {
            return false;
        }
    }

    // 更新邮箱索引
    if (!user.email.empty()) {
        std::string email_idx_path = email_index_path();
        JsonValue email_idx = json_object();
        if (file_exists(email_idx_path)) {
            JsonParser parser;
            auto existing = parser.parse(read_file(email_idx_path));
            if (existing && existing->is_object()) {
                email_idx = std::move(*existing);
            }
        }
        email_idx.object_insert(user.email, JsonValue(user.user_id));
        if (!write_file(email_idx_path, email_idx.serialize(true))) {
            return false;
        }
    }

    return true;
}

bool Database::update_user(const UserData& user)
{
    return save_user(user);
}

bool Database::delete_user(const std::string& user_id)
{
    auto user = get_user(user_id);
    if (!user) {
        return false;
    }

    std::error_code ec;
    if (!fs::remove(user_path(user_id), ec)) {
        return false;
    }

    // 从用户名索引删除
    {
        std::string idx_path = username_index_path();
        if (file_exists(idx_path)) {
            JsonParser parser;
            auto idx_json = parser.parse(read_file(idx_path));
            if (idx_json && idx_json->is_object()) {
                // 创建新对象排除该键
                JsonValue new_idx = json_object();
                for (auto it = idx_json->object_begin(); it != idx_json->object_end(); ++it) {
                    if (it->first != user->username) {
                        new_idx.object_insert(it->first, it->second);
                    }
                }
                write_file(idx_path, new_idx.serialize(true));
            }
        }
    }

    // 从邮箱索引删除
    if (!user->email.empty()) {
        std::string email_idx_path = email_index_path();
        if (file_exists(email_idx_path)) {
            JsonParser parser;
            auto idx_json = parser.parse(read_file(email_idx_path));
            if (idx_json && idx_json->is_object()) {
                JsonValue new_idx = json_object();
                for (auto it = idx_json->object_begin(); it != idx_json->object_end(); ++it) {
                    if (it->first != user->email) {
                        new_idx.object_insert(it->first, it->second);
                    }
                }
                write_file(email_idx_path, new_idx.serialize(true));
            }
        }
    }

    return true;
}

std::vector<UserData> Database::search_users(const std::string& keyword, int limit)
{
    std::vector<UserData> results;
    std::string dir = data_path_ + "/users/";
    if (!fs::exists(dir)) {
        return results;
    }

    int count = 0;
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (count >= limit) break;
        if (!entry.is_regular_file()) continue;
        std::string filename = entry.path().filename().string();
        if (filename == "username_index.json" ||
            filename == "email_index.json" ||
            filename == "counter.json") {
            continue;
        }
        if (filename.size() < 5 || filename.substr(filename.size() - 5) != ".json") {
            continue;
        }
        std::string content = read_file(entry.path().string());
        JsonParser parser;
        auto json_val = parser.parse(content);
        if (!json_val || !json_val->is_object()) continue;

        std::string username = (*json_val)["username"].get_string("");
        std::string nickname = (*json_val)["nickname"].get_string("");
        if (username.find(keyword) != std::string::npos ||
            nickname.find(keyword) != std::string::npos) {
            UserData user;
            user.user_id       = (*json_val)["user_id"].get_string("");
            user.username      = username;
            user.nickname      = nickname;
            user.password_hash = (*json_val)["password_hash"].get_string("");
            user.email         = (*json_val)["email"].get_string("");
            user.avatar_path   = (*json_val)["avatar_path"].get_string("");
            user.created_at    = (*json_val)["created_at"].get_string("");
            user.bio           = (*json_val)["bio"].get_string("");
            results.push_back(std::move(user));
            count++;
        }
    }
    return results;
}

// ============================================================
// 消息操作
// ============================================================
std::optional<MessageData> Database::get_message(const std::string& message_id)
{
    std::string path = message_path(message_id);
    if (!file_exists(path)) {
        return std::nullopt;
    }
    std::string content = read_file(path);
    JsonParser parser;
    auto json = parser.parse(content);
    if (!json || !json->is_object()) {
        return std::nullopt;
    }

    MessageData msg;
    msg.message_id = (*json)["message_id"].get_string("");
    msg.from_id    = (*json)["from_id"].get_string("");
    msg.to_id      = (*json)["to_id"].get_string("");
    msg.content    = (*json)["content"].get_string("");
    msg.timestamp  = static_cast<uint64_t>((*json)["timestamp"].get_double(0.0));
    msg.msg_type   = static_cast<int>((*json)["msg_type"].get_double(0.0));
    return msg;
}

bool Database::save_message(const MessageData& msg)
{
    JsonValue obj = json_object();
    obj.object_insert("message_id", JsonValue(msg.message_id));
    obj.object_insert("from_id",    JsonValue(msg.from_id));
    obj.object_insert("to_id",      JsonValue(msg.to_id));
    obj.object_insert("content",    JsonValue(msg.content));
    obj.object_insert("timestamp",  JsonValue(static_cast<double>(msg.timestamp)));
    obj.object_insert("msg_type",   JsonValue(static_cast<double>(msg.msg_type)));

    return write_file(message_path(msg.message_id), obj.serialize(true));
}

std::vector<MessageData> Database::get_messages(
    const std::string& user_id,
    const std::string& other_id,
    int limit, int offset)
{
    std::vector<MessageData> results;
    std::string dir = data_path_ + "/messages/";
    if (!fs::exists(dir)) {
        return results;
    }

    for (const auto& entry : fs::directory_iterator(dir)) {
        if (!entry.is_regular_file()) continue;
        std::string filename = entry.path().filename().string();
        if (filename.size() < 5 || filename.substr(filename.size() - 5) != ".json") {
            continue;
        }
        std::string content = read_file(entry.path().string());
        JsonParser parser;
        auto json = parser.parse(content);
        if (!json || !json->is_object()) continue;

        std::string from = (*json)["from_id"].get_string("");
        std::string to   = (*json)["to_id"].get_string("");

        bool match = (from == user_id && to == other_id) ||
                     (from == other_id && to == user_id);
        if (!match) continue;

        MessageData msg;
        msg.message_id = (*json)["message_id"].get_string("");
        msg.from_id    = from;
        msg.to_id      = to;
        msg.content    = (*json)["content"].get_string("");
        msg.timestamp  = static_cast<uint64_t>((*json)["timestamp"].get_double(0.0));
        msg.msg_type   = static_cast<int>((*json)["msg_type"].get_double(0.0));
        results.push_back(std::move(msg));
    }

    // 按时间戳排序 (最新的在前)
    std::sort(results.begin(), results.end(),
              [](const MessageData& a, const MessageData& b) {
                  return a.timestamp > b.timestamp;
              });

    // 分页
    if (offset > 0 && static_cast<size_t>(offset) < results.size()) {
        results.erase(results.begin(), results.begin() + offset);
    }
    if (static_cast<size_t>(limit) < results.size()) {
        results.resize(static_cast<size_t>(limit));
    }

    // 反转回时间正序
    std::reverse(results.begin(), results.end());
    return results;
}

// ============================================================
// 好友操作
// ============================================================
bool Database::add_friend(const std::string& user_id, const std::string& friend_id)
{
    std::string path = friendship_path(user_id);
    JsonValue arr = json_array();
    if (file_exists(path)) {
        JsonParser parser;
        auto existing = parser.parse(read_file(path));
        if (existing && existing->is_array()) {
            arr = std::move(*existing);
        }
    }

    // 检查是否已存在
    for (size_t i = 0; i < arr.array_size(); i++) {
        if (arr[i].get_string("friend_id") == friend_id) {
            LOG_WARN("[DB] Friendship already exists: %s -> %s",
                     user_id.c_str(), friend_id.c_str());
            return false;
        }
    }

    // 添加新好友关系
    JsonValue item = json_object();
    item.object_insert("user_id",    JsonValue(user_id));
    item.object_insert("friend_id",  JsonValue(friend_id));
    item.object_insert("status",     JsonValue(std::string("pending")));
    item.object_insert("created_at", JsonValue(static_cast<double>(
        static_cast<uint64_t>(std::time(nullptr)))));
    arr.array_push_back(std::move(item));

    return write_file(path, arr.serialize(true));
}

bool Database::remove_friend(const std::string& user_id, const std::string& friend_id)
{
    std::string path = friendship_path(user_id);
    if (!file_exists(path)) return false;

    JsonParser parser;
    auto existing = parser.parse(read_file(path));
    if (!existing || !existing->is_array()) return false;

    JsonValue new_arr = json_array();
    bool found = false;
    for (size_t i = 0; i < existing->array_size(); i++) {
        if ((*existing)[i].get_string("friend_id") == friend_id) {
            found = true;
            continue;
        }
        new_arr.array_push_back((*existing)[i]);
    }
    if (!found) return false;

    return write_file(path, new_arr.serialize(true));
}

bool Database::accept_friend(const std::string& user_id, const std::string& friend_id)
{
    // 更新 user_id 的好友状态
    std::string path = friendship_path(user_id);
    if (!file_exists(path)) return false;

    JsonParser parser;
    auto existing = parser.parse(read_file(path));
    if (!existing || !existing->is_array()) return false;

    bool updated = false;
    for (size_t i = 0; i < existing->array_size(); i++) {
        if ((*existing)[i].get_string("friend_id") == friend_id) {
            std::string prev_status = (*existing)[i].get_string("status");
            // JsonValue 是 const 不可修改，需要重建数组
            updated = true;
            break;
        }
    }

    if (!updated) return false;

    // 重建好友列表（因为 JsonValue operator[] 返回 const 引用，不能直接修改）
    JsonValue new_arr = json_array();
    for (size_t i = 0; i < existing->array_size(); i++) {
        auto& item = (*existing)[i];
        if (item.get_string("friend_id") == friend_id) {
            // 替换为 accepted
            JsonValue new_item = json_object();
            new_item.object_insert("user_id",    JsonValue(item["user_id"].get_string("")));
            new_item.object_insert("friend_id",  JsonValue(friend_id));
            new_item.object_insert("status",     JsonValue(std::string("accepted")));
            new_item.object_insert("created_at", JsonValue(item["created_at"].get_double(0.0)));
            new_arr.array_push_back(std::move(new_item));
        } else {
            new_arr.array_push_back(item);
        }
    }
    if (!write_file(path, new_arr.serialize(true))) return false;

    // 添加反向好友关系
    if (is_friend(friend_id, user_id)) {
        return true;
    }

    std::string rev_path = friendship_path(friend_id);
    JsonValue rev_arr = json_array();
    if (file_exists(rev_path)) {
        JsonParser rev_parser;
        auto rev_existing = rev_parser.parse(read_file(rev_path));
        if (rev_existing && rev_existing->is_array()) {
            rev_arr = std::move(*rev_existing);
        }
    }
    JsonValue rev_item = json_object();
    rev_item.object_insert("user_id",    JsonValue(friend_id));
    rev_item.object_insert("friend_id",  JsonValue(user_id));
    rev_item.object_insert("status",     JsonValue(std::string("accepted")));
    rev_item.object_insert("created_at", JsonValue(static_cast<double>(
        static_cast<uint64_t>(std::time(nullptr)))));
    rev_arr.array_push_back(std::move(rev_item));

    return write_file(rev_path, rev_arr.serialize(true));
}

std::vector<FriendshipData> Database::get_friends(const std::string& user_id)
{
    std::vector<FriendshipData> results;
    std::string path = friendship_path(user_id);
    if (!file_exists(path)) return results;

    JsonParser parser;
    auto existing = parser.parse(read_file(path));
    if (!existing || !existing->is_array()) return results;

    for (size_t i = 0; i < existing->array_size(); i++) {
        auto& item = (*existing)[i];
        FriendshipData fd;
        fd.user_id    = item["user_id"].get_string("");
        fd.friend_id  = item["friend_id"].get_string("");
        fd.status     = item["status"].get_string("pending");
        fd.created_at = static_cast<uint64_t>(item["created_at"].get_double(0.0));
        results.push_back(std::move(fd));
    }
    return results;
}

bool Database::is_friend(const std::string& user_id, const std::string& friend_id)
{
    auto friends = get_friends(user_id);
    for (const auto& f : friends) {
        if (f.friend_id == friend_id && f.status == "accepted") {
            return true;
        }
    }
    return false;
}

// ============================================================
// 模板操作
// ============================================================
bool Database::save_template(const TemplateData& tmpl)
{
    JsonValue obj = json_object();
    obj.object_insert("template_id", JsonValue(tmpl.template_id));
    obj.object_insert("name",        JsonValue(tmpl.name));
    obj.object_insert("content",     JsonValue(tmpl.content));
    obj.object_insert("author_id",   JsonValue(tmpl.author_id));
    obj.object_insert("created_at",  JsonValue(static_cast<double>(tmpl.created_at)));
    return write_file(template_path(tmpl.template_id), obj.serialize(true));
}

std::optional<TemplateData> Database::get_template(const std::string& template_id)
{
    std::string path = template_path(template_id);
    if (!file_exists(path)) return std::nullopt;

    JsonParser parser;
    auto json = parser.parse(read_file(path));
    if (!json || !json->is_object()) return std::nullopt;

    TemplateData tmpl;
    tmpl.template_id = (*json)["template_id"].get_string("");
    tmpl.name        = (*json)["name"].get_string("");
    tmpl.content     = (*json)["content"].get_string("");
    tmpl.author_id   = (*json)["author_id"].get_string("");
    tmpl.created_at  = static_cast<uint64_t>((*json)["created_at"].get_double(0.0));
    return tmpl;
}

std::vector<TemplateData> Database::list_templates(int limit, int offset)
{
    std::vector<TemplateData> results;
    std::string dir = data_path_ + "/templates/";
    if (!fs::exists(dir)) return results;

    for (const auto& entry : fs::directory_iterator(dir)) {
        if (!entry.is_regular_file()) continue;
        std::string filename = entry.path().filename().string();
        if (filename.size() < 5 || filename.substr(filename.size() - 5) != ".json") {
            continue;
        }
        auto tmpl = get_template(filename.substr(0, filename.size() - 5));
        if (tmpl) {
            results.push_back(std::move(*tmpl));
        }
    }

    std::sort(results.begin(), results.end(),
              [](const TemplateData& a, const TemplateData& b) {
                  return a.created_at > b.created_at;
              });

    if (offset > 0 && static_cast<size_t>(offset) < results.size()) {
        results.erase(results.begin(), results.begin() + offset);
    }
    if (static_cast<size_t>(limit) < results.size()) {
        results.resize(static_cast<size_t>(limit));
    }
    return results;
}

// ============================================================
// OAuth 账号绑定
// ============================================================
bool Database::save_oauth_account(const std::string& platform,
                                   const std::string& open_id,
                                   const std::string& user_id)
{
    std::string path = oauth_index_path(platform);
    JsonValue obj = json_object();
    if (file_exists(path)) {
        JsonParser parser;
        auto existing = parser.parse(read_file(path));
        if (existing && existing->is_object()) {
            obj = std::move(*existing);
        }
    }
    obj.object_insert(open_id, JsonValue(user_id));
    return write_file(path, obj.serialize(true));
}

std::optional<std::string> Database::get_user_by_oauth(
    const std::string& platform,
    const std::string& open_id)
{
    std::string path = oauth_index_path(platform);
    if (!file_exists(path)) return std::nullopt;

    JsonParser parser;
    auto json = parser.parse(read_file(path));
    if (!json || !json->is_object()) return std::nullopt;

    std::string user_id = (*json)[open_id].get_string("");
    if (user_id.empty()) return std::nullopt;
    return user_id;
}

bool Database::remove_oauth_account(const std::string& platform,
                                     const std::string& open_id)
{
    std::string path = oauth_index_path(platform);
    if (!file_exists(path)) return false;

    JsonParser parser;
    auto json = parser.parse(read_file(path));
    if (!json || !json->is_object()) return false;

    JsonValue new_obj = json_object();
    for (auto it = json->object_begin(); it != json->object_end(); ++it) {
        if (it->first != open_id) {
            new_obj.object_insert(it->first, it->second);
        }
    }
    return write_file(path, new_obj.serialize(true));
}

} // namespace db
} // namespace chrono
