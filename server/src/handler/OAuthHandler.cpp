/**
 * Chrono-shift C++ OAuth 登录处理器实现
 */
#include "OAuthHandler.h"
#include "../ffi/RustBridge.h"
#include "../security/SecurityManager.h"
#include "../util/Logger.h"
#include "../util/StringUtils.h"

namespace chrono {
namespace handler {

using namespace chrono::json;

OAuthHandler::OAuthHandler(db::Database& database)
    : db_(database)
{
}

// ============================================================
// QQ 登录
// ============================================================
JsonValue OAuthHandler::handle_qq_login(const JsonValue& params)
{
    return oauth_login_impl("qq", params);
}

// ============================================================
// 微信登录
// ============================================================
JsonValue OAuthHandler::handle_wechat_login(const JsonValue& params)
{
    return oauth_login_impl("wechat", params);
}

// ============================================================
// 邮箱登录
// ============================================================
JsonValue OAuthHandler::handle_email_login(const JsonValue& params)
{
    std::string email    = params["email"].get_string("");
    std::string password = params["password"].get_string("");

    if (email.empty() || password.empty()) {
        return build_error("邮箱和密码不能为空");
    }

    // 通过邮箱查找用户
    auto user_opt = db_.get_user_by_email(email);
    if (!user_opt) {
        return build_error("邮箱未注册");
    }

    // 验证密码
    bool verified = ffi::RustBridge::verify_password(password, user_opt->password_hash);
    if (!verified) {
        return build_error("邮箱或密码错误");
    }

    // 生成 JWT
    std::string token = ffi::RustBridge::generate_jwt(user_opt->user_id);
    if (token.empty()) {
        return build_error("令牌生成失败");
    }

    JsonValue data = json_object();
    data.object_insert("user_id",  JsonValue(user_opt->user_id));
    data.object_insert("username", JsonValue(user_opt->username));
    data.object_insert("email",    JsonValue(user_opt->email));
    data.object_insert("nickname", JsonValue(user_opt->nickname));
    data.object_insert("token",    JsonValue(token));

    return build_success(data);
}

// ============================================================
// 邮箱注册
// ============================================================
JsonValue OAuthHandler::handle_email_register(const JsonValue& params)
{
    std::string email    = params["email"].get_string("");
    std::string password = params["password"].get_string("");
    std::string code     = params["code"].get_string("");     // 验证码 (简化版)
    std::string username = params["username"].get_string("");

    if (email.empty() || password.empty()) {
        return build_error("邮箱和密码不能为空");
    }

    // 验证邮箱格式
    if (!security::InputSanitizer::is_valid_email(email)) {
        return build_error("无效的邮箱格式");
    }

    // 验证密码强度
    std::string pwd_error = security::InputSanitizer::check_password_strength(password);
    if (!pwd_error.empty()) {
        return build_error(pwd_error);
    }

    // 检查邮箱是否已注册
    if (db_.get_user_by_email(email).has_value()) {
        return build_error("邮箱已被注册");
    }

    // 验证码检查 (简化版，生产环境应使用邮件验证码服务)
    // if (code != "123456") { return build_error("验证码错误"); }

    // 生成用户名 (如果未提供，使用邮箱前缀)
    if (username.empty()) {
        size_t at_pos = email.find('@');
        username = (at_pos != std::string::npos) ? email.substr(0, at_pos) : email;
    }
    username = security::InputSanitizer::sanitize_username(username);
    if (username.empty()) {
        username = "user_" + util::StringUtils::generate_uuid().substr(0, 8);
    }

    // 检查用户名是否已存在
    if (db_.get_user_by_username(username).has_value()) {
        // 追加随机后缀
        username += "_" + util::StringUtils::generate_uuid().substr(0, 4);
    }

    // 密码哈希
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
    user.created_at    = util::StringUtils::timestamp_ms();

    if (!db_.save_user(user)) {
        return build_error("注册失败");
    }

    // 生成 JWT
    std::string token = ffi::RustBridge::generate_jwt(user.user_id);
    if (token.empty()) {
        return build_error("令牌生成失败");
    }

    JsonValue data = json_object();
    data.object_insert("user_id",  JsonValue(user.user_id));
    data.object_insert("username", JsonValue(user.username));
    data.object_insert("email",    JsonValue(user.email));
    data.object_insert("token",    JsonValue(token));

    return build_success(data);
}

// ============================================================
// 绑定/解绑 OAuth
// ============================================================
JsonValue OAuthHandler::handle_bind_oauth(const JsonValue& params)
{
    std::string user_id  = params["user_id"].get_string("");
    std::string platform = params["platform"].get_string("");
    std::string open_id  = params["open_id"].get_string("");

    if (user_id.empty() || platform.empty() || open_id.empty()) {
        return build_error("参数不完整");
    }

    if (!db_.save_oauth_account(platform, open_id, user_id)) {
        return build_error("绑定失败");
    }

    return build_success(JsonValue(json_object()));
}

JsonValue OAuthHandler::handle_unbind_oauth(const JsonValue& params)
{
    std::string platform = params["platform"].get_string("");
    std::string open_id  = params["open_id"].get_string("");

    if (platform.empty() || open_id.empty()) {
        return build_error("参数不完整");
    }

    if (!db_.remove_oauth_account(platform, open_id)) {
        return build_error("解绑失败");
    }

    return build_success(JsonValue(json_object()));
}

// ============================================================
// 通用 OAuth 登录实现
// ============================================================
JsonValue OAuthHandler::oauth_login_impl(const std::string& platform,
                                          const JsonValue& params)
{
    std::string open_id      = params["open_id"].get_string("");
    std::string access_token = params["access_token"].get_string("");
    std::string nickname     = params["nickname"].get_string("");

    if (open_id.empty() || access_token.empty()) {
        return build_error("参数不完整");
    }

    // 验证 OAuth token (简化版)
    if (!verify_oauth_token(platform, open_id, access_token)) {
        return build_error("OAuth 验证失败");
    }

    // 查找是否已有绑定的账号
    auto user_id_opt = db_.get_user_by_oauth(platform, open_id);
    if (user_id_opt) {
        // 已有绑定，直接登录
        auto user_opt = db_.get_user(*user_id_opt);
        if (!user_opt) {
            return build_error("用户数据异常");
        }

        std::string token = ffi::RustBridge::generate_jwt(user_opt->user_id);
        if (token.empty()) {
            return build_error("令牌生成失败");
        }

        JsonValue data = json_object();
        data.object_insert("user_id",  JsonValue(user_opt->user_id));
        data.object_insert("username", JsonValue(user_opt->username));
        data.object_insert("nickname", JsonValue(user_opt->nickname));
        data.object_insert("token",    JsonValue(token));
        data.object_insert("is_new",   JsonValue(false));

        return build_success(data);
    }

    // 首次登录：创建新用户并绑定
    std::string username = "user_" + util::StringUtils::generate_uuid().substr(0, 8);
    if (!nickname.empty()) {
        username = security::InputSanitizer::sanitize_username(nickname);
        if (username.empty()) {
            username = "user_" + util::StringUtils::generate_uuid().substr(0, 8);
        }
        // 检查用户名是否已存在
        if (db_.get_user_by_username(username).has_value()) {
            username += "_" + util::StringUtils::generate_uuid().substr(0, 4);
        }
    }

    db::UserData user;
    user.user_id    = db_.generate_user_id();
    user.username   = username;
    user.nickname   = nickname.empty() ? username : nickname;
    user.created_at = util::StringUtils::timestamp_ms();

    if (!db_.save_user(user)) {
        return build_error("创建用户失败");
    }

    // 绑定 OAuth 账号
    if (!db_.save_oauth_account(platform, open_id, user.user_id)) {
        LOG_ERROR("[OAuth] Failed to bind %s account %s to user %s",
                  platform.c_str(), open_id.c_str(), user.user_id.c_str());
    }

    // 生成 JWT
    std::string token = ffi::RustBridge::generate_jwt(user.user_id);
    if (token.empty()) {
        return build_error("令牌生成失败");
    }

    JsonValue data = json_object();
    data.object_insert("user_id",  JsonValue(user.user_id));
    data.object_insert("username", JsonValue(user.username));
    data.object_insert("nickname", JsonValue(user.nickname));
    data.object_insert("token",    JsonValue(token));
    data.object_insert("is_new",   JsonValue(true));

    return build_success(data);
}

bool OAuthHandler::verify_oauth_token(const std::string& platform,
                                       const std::string& open_id,
                                       const std::string& access_token)
{
    // 简化版验证：仅检查 token 不为空且格式基本正确
    // 生产环境应调用 QQ/微信的 API 验证 access_token
    if (access_token.empty()) return false;
    if (access_token.size() < 6) return false;

    // QQ 登录验证 URL:
    // https://graph.qq.com/oauth2.0/me?access_token=ACCESS_TOKEN
    //
    // 微信登录验证 URL:
    // https://api.weixin.qq.com/sns/userinfo?access_token=ACCESS_TOKEN&openid=OPENID

    LOG_INFO("[OAuth] %s login: open_id=%s, token_len=%zu (verification simulated)",
             platform.c_str(), open_id.c_str(), access_token.size());

    return true;
}

} // namespace handler
} // namespace chrono
