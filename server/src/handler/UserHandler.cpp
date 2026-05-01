/**
 * Chrono-shift C++ 用户处理器实现
 */
#include "UserHandler.h"
#include "../ffi/RustBridge.h"
#include "../security/SecurityManager.h"
#include "../util/Logger.h"
#include "../util/StringUtils.h"

namespace chrono {
namespace handler {

using namespace chrono::json;

UserHandler::UserHandler(db::Database& database)
    : db_(database)
{
}

JsonValue UserHandler::handle_register(const JsonValue& params)
{
    std::string username = params["username"].get_string("");
    std::string password = params["password"].get_string("");
    std::string email    = params["email"].get_string("");

    // 输入验证
    username = security::InputSanitizer::sanitize_username(username);
    if (username.empty()) {
        return build_error("无效的用户名");
    }
    if (password.empty()) {
        return build_error("密码不能为空");
    }

    std::string pwd_error = security::InputSanitizer::check_password_strength(password);
    if (!pwd_error.empty()) {
        return build_error(pwd_error);
    }

    if (!email.empty() && !security::InputSanitizer::is_valid_email(email)) {
        return build_error("无效的邮箱格式");
    }

    // 检查用户名是否已存在
    if (db_.get_user_by_username(username).has_value()) {
        return build_error("用户名已存在");
    }

    // 检查邮箱是否已存在
    if (!email.empty() && db_.get_user_by_email(email).has_value()) {
        return build_error("邮箱已被注册");
    }

    // 密码哈希 (通过 Rust FFI Argon2id)
    std::string password_hash = ffi::RustBridge::hash_password(password);
    if (password_hash.empty()) {
        return build_error("密码加密失败");
    }

    // 创建用户
    db::UserData user;
    user.user_id       = db_.generate_user_id();
    user.username      = username;
    user.nickname      = username;
    user.password_hash = password_hash;
    user.email         = email;
    user.created_at    = chrono::util::StringUtils::timestamp_ms();

    if (!db_.save_user(user)) {
        return build_error("保存用户失败");
    }

    // 生成 JWT
    std::string token = ffi::RustBridge::generate_jwt(user.user_id);
    if (token.empty()) {
        return build_error("令牌生成失败");
    }

    JsonValue data = json_object();
    data.object_insert("user_id",  JsonValue(user.user_id));
    data.object_insert("username", JsonValue(user.username));
    data.object_insert("token",    JsonValue(token));

    return build_success(data);
}

JsonValue UserHandler::handle_login(const JsonValue& params)
{
    std::string username = params["username"].get_string("");
    std::string password = params["password"].get_string("");

    if (username.empty() || password.empty()) {
        return build_error("用户名和密码不能为空");
    }

    // 查找用户
    auto user_opt = db_.get_user_by_username(username);
    if (!user_opt) {
        return build_error("用户名或密码错误");
    }

    // 验证密码
    bool verified = ffi::RustBridge::verify_password(password, user_opt->password_hash);
    if (!verified) {
        return build_error("用户名或密码错误");
    }

    // 生成 JWT
    std::string token = ffi::RustBridge::generate_jwt(user_opt->user_id);
    if (token.empty()) {
        return build_error("令牌生成失败");
    }

    JsonValue data = json_object();
    data.object_insert("user_id",  JsonValue(user_opt->user_id));
    data.object_insert("username", JsonValue(user_opt->username));
    data.object_insert("nickname", JsonValue(user_opt->nickname));
    data.object_insert("email",    JsonValue(user_opt->email));
    data.object_insert("avatar",   JsonValue(user_opt->avatar_path));
    data.object_insert("token",    JsonValue(token));

    return build_success(data);
}

JsonValue UserHandler::handle_get_profile(const JsonValue& params)
{
    std::string user_id = params["user_id"].get_string("");
    if (user_id.empty()) {
        return build_error("用户 ID 不能为空");
    }

    auto user_opt = db_.get_user(user_id);
    if (!user_opt) {
        return build_error("用户不存在");
    }

    JsonValue data = json_object();
    data.object_insert("user_id",    JsonValue(user_opt->user_id));
    data.object_insert("username",   JsonValue(user_opt->username));
    data.object_insert("nickname",   JsonValue(user_opt->nickname));
    data.object_insert("email",      JsonValue(user_opt->email));
    data.object_insert("avatar",     JsonValue(user_opt->avatar_path));
    data.object_insert("created_at", JsonValue(user_opt->created_at));
    data.object_insert("bio",        JsonValue(user_opt->bio));

    return build_success(data);
}

JsonValue UserHandler::handle_update_profile(const JsonValue& params)
{
    std::string user_id  = params["user_id"].get_string("");
    std::string nickname = params["nickname"].get_string("");
    std::string bio      = params["bio"].get_string("");

    if (user_id.empty()) {
        return build_error("用户 ID 不能为空");
    }

    auto user_opt = db_.get_user(user_id);
    if (!user_opt) {
        return build_error("用户不存在");
    }

    if (!nickname.empty()) {
        user_opt->nickname = security::InputSanitizer::sanitize_display_name(nickname);
    }
    if (!bio.empty()) {
        user_opt->bio = security::InputSanitizer::sanitize_message(bio);
    }

    if (!db_.update_user(*user_opt)) {
        return build_error("更新失败");
    }

    return build_success(JsonValue(json_object()));
}

JsonValue UserHandler::handle_search(const JsonValue& params)
{
    std::string keyword = params["keyword"].get_string("");
    if (keyword.empty()) {
        return build_error("关键词不能为空");
    }

    auto users = db_.search_users(keyword);
    JsonValue arr = json_array();
    for (const auto& u : users) {
        JsonValue item = json_object();
        item.object_insert("user_id",  JsonValue(u.user_id));
        item.object_insert("username", JsonValue(u.username));
        item.object_insert("nickname", JsonValue(u.nickname));
        item.object_insert("avatar",   JsonValue(u.avatar_path));
        arr.array_push_back(std::move(item));
    }

    return build_success(arr);
}

} // namespace handler
} // namespace chrono
