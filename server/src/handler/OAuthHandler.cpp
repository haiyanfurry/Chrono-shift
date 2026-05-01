/**
 * Chrono-shift C++ OAuth 登录处理器实现
 * QQ / 微信授权码流程 + 邮箱验证码
 */
#include "OAuthHandler.h"
#include "../ffi/RustBridge.h"
#include "../security/SecurityManager.h"
#include "../util/Logger.h"
#include "../util/StringUtils.h"

#include <cstdlib>
#include <ctime>
#include <sstream>

namespace chrono {
namespace handler {

using namespace chrono::json;

// ============================================================
// 构造函数
// ============================================================

OAuthHandler::OAuthHandler(db::Database& database)
    : db_(database)
{
}

OAuthHandler::OAuthHandler(db::Database& database, const OAuthConfig& config)
    : db_(database)
{
    init_oauth_config(config);
}

void OAuthHandler::init_oauth_config(const OAuthConfig& config)
{
    config_ = config;
    configured_ = true;

    // 创建 QQ 客户端 (如果配置了 app_id)
    if (!config.qq.app_id.empty()) {
        qq_client_ = std::make_unique<security::QQClient>(config.qq);
        LOG_INFO("[OAuth] QQ client initialized: app_id=%s",
                 config.qq.app_id.c_str());
    } else {
        qq_client_.reset();
        LOG_WARN("[OAuth] QQ client not configured (app_id empty)");
    }

    // 创建微信客户端
    if (!config.wechat.app_id.empty()) {
        wechat_client_ = std::make_unique<security::WechatClient>(config.wechat);
        LOG_INFO("[OAuth] WeChat client initialized: app_id=%s",
                 config.wechat.app_id.c_str());
    } else {
        wechat_client_.reset();
        LOG_WARN("[OAuth] WeChat client not configured (app_id empty)");
    }

    // 创建邮件验证码发送器
    if (!config.smtp.username.empty()) {
        email_verifier_ = std::make_unique<security::EmailVerifier>(config.smtp);
        LOG_INFO("[OAuth] Email verifier initialized: smtp=%s:%d, from=%s",
                 config.smtp.host.c_str(), config.smtp.port,
                 config.smtp.from_addr.c_str());
    } else {
        email_verifier_.reset();
        LOG_WARN("[OAuth] Email verifier not configured (SMTP username empty)");
    }
}

// ============================================================
// QQ 登录 (旧 API — 直接传 access_token + open_id)
// ============================================================
JsonValue OAuthHandler::handle_qq_login(const JsonValue& params)
{
    return oauth_login_impl("qq", params);
}

// ============================================================
// 微信登录 (旧 API)
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
// 邮箱注册 (含验证码)
// ============================================================
JsonValue OAuthHandler::handle_email_register(const JsonValue& params)
{
    std::string email    = params["email"].get_string("");
    std::string password = params["password"].get_string("");
    std::string code     = params["code"].get_string("");
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

    // 验证码检查
    if (!code.empty()) {
        std::lock_guard<std::mutex> lock(code_mutex_);
        auto it = email_codes_.find(email);
        if (it == email_codes_.end()) {
            return build_error("验证码未发送或已过期");
        }
        uint64_t now = util::StringUtils::timestamp_ms();
        if (now - it->second.created_at_ms > kCodeExpiryMs) {
            email_codes_.erase(it);
            return build_error("验证码已过期，请重新获取");
        }
        if (it->second.code != code) {
            return build_error("验证码错误");
        }
        // 验证通过，删除缓存的验证码
        email_codes_.erase(it);
    } else if (email_verifier_) {
        // 配置了邮件服务但未传验证码 — 要求先获取验证码
        return build_error("请先获取邮箱验证码");
    }
    // 如果既没有验证码也没有配置邮件服务，允许无验证码注册 (兼容模式)

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
// P9.2 — 获取 QQ 授权 URL
// ============================================================
JsonValue OAuthHandler::handle_qq_auth_url(const JsonValue& params)
{
    if (!qq_client_) {
        return build_error("QQ 登录尚未配置");
    }

    std::string state = params["state"].get_string("");
    if (state.empty()) {
        state = generate_state();
    }

    std::string auth_url = qq_client_->build_auth_url(state);

    JsonValue data = json_object();
    data.object_insert("url",    JsonValue(auth_url));
    data.object_insert("state",  JsonValue(state));
    data.object_insert("platform", JsonValue(std::string("qq")));

    return build_success(data);
}

// ============================================================
// P9.2 — QQ 授权回调
// ============================================================
JsonValue OAuthHandler::handle_qq_callback(const JsonValue& params)
{
    if (!qq_client_) {
        return build_error("QQ 登录尚未配置");
    }

    std::string code  = params["code"].get_string("");
    std::string state = params["state"].get_string("");

    if (code.empty()) {
        return build_error("缺少授权码 (code)");
    }

    // 1. 用 code 换取 access_token 和 open_id
    std::string access_token;
    std::string open_id;
    if (!qq_client_->exchange_code(code, access_token, open_id)) {
        return build_error("QQ 授权码交换失败");
    }

    if (open_id.empty()) {
        return build_error("获取 QQ open_id 失败");
    }

    // 2. 获取用户信息
    security::OAuthUserInfo user_info;
    if (!qq_client_->get_user_info(access_token, open_id, user_info)) {
        LOG_WARN("[OAuth] QQ get_user_info failed for open_id=%s, proceeding...",
                 open_id.c_str());
        // 即使获取用户信息失败，仍可继续登录
    }

    // 3. 构造参数并执行统一登录逻辑
    JsonValue login_params = json_object();
    login_params.object_insert("open_id",      JsonValue(open_id));
    login_params.object_insert("access_token", JsonValue(access_token));
    login_params.object_insert("nickname",     JsonValue(user_info.nickname.empty()
                                        ? "qq_user" : user_info.nickname));

    return oauth_login_impl("qq", login_params);
}

// ============================================================
// P9.2 — 获取微信授权 URL
// ============================================================
JsonValue OAuthHandler::handle_wechat_auth_url(const JsonValue& params)
{
    if (!wechat_client_) {
        return build_error("微信登录尚未配置");
    }

    std::string state = params["state"].get_string("");
    if (state.empty()) {
        state = generate_state();
    }

    std::string auth_url = wechat_client_->build_auth_url(state);

    JsonValue data = json_object();
    data.object_insert("url",    JsonValue(auth_url));
    data.object_insert("state",  JsonValue(state));
    data.object_insert("platform", JsonValue(std::string("wechat")));

    return build_success(data);
}

// ============================================================
// P9.2 — 微信授权回调
// ============================================================
JsonValue OAuthHandler::handle_wechat_callback(const JsonValue& params)
{
    if (!wechat_client_) {
        return build_error("微信登录尚未配置");
    }

    std::string code  = params["code"].get_string("");
    std::string state = params["state"].get_string("");

    if (code.empty()) {
        return build_error("缺少授权码 (code)");
    }

    // 1. 用 code 换取 access_token 和 open_id
    std::string access_token;
    std::string open_id;
    if (!wechat_client_->exchange_code(code, access_token, open_id)) {
        return build_error("微信授权码交换失败");
    }

    if (open_id.empty()) {
        return build_error("获取微信 open_id 失败");
    }

    // 2. 获取用户信息
    security::OAuthUserInfo user_info;
    if (!wechat_client_->get_user_info(access_token, open_id, user_info)) {
        LOG_WARN("[OAuth] WeChat get_user_info failed for open_id=%s, proceeding...",
                 open_id.c_str());
    }

    // 3. 执行统一登录逻辑
    JsonValue login_params = json_object();
    login_params.object_insert("open_id",      JsonValue(open_id));
    login_params.object_insert("access_token", JsonValue(access_token));
    login_params.object_insert("nickname",     JsonValue(user_info.nickname.empty()
                                        ? "wechat_user" : user_info.nickname));

    return oauth_login_impl("wechat", login_params);
}

// ============================================================
// P9.2 — 发送邮箱验证码
// ============================================================
JsonValue OAuthHandler::handle_send_email_code(const JsonValue& params)
{
    if (!email_verifier_) {
        return build_error("邮件服务尚未配置");
    }

    std::string email = params["email"].get_string("");
    if (email.empty()) {
        return build_error("邮箱不能为空");
    }

    // 验证邮箱格式
    if (!security::InputSanitizer::is_valid_email(email)) {
        return build_error("无效的邮箱格式");
    }

    // 生成验证码
    std::string code = generate_code();

    // 发送验证码
    if (!email_verifier_->send_code(email, code)) {
        return build_error("验证码发送失败，请稍后重试");
    }

    // 缓存验证码
    {
        std::lock_guard<std::mutex> lock(code_mutex_);
        cleanup_expired_codes();
        EmailCodeEntry entry;
        entry.code          = code;
        entry.created_at_ms = util::StringUtils::timestamp_ms();
        email_codes_[email] = entry;
    }

    LOG_INFO("[OAuth] Email code sent to %s", email.c_str());

    JsonValue data = json_object();
    data.object_insert("email",   JsonValue(email));
    data.object_insert("message", JsonValue(std::string("验证码已发送到您的邮箱")));

    return build_success(data);
}

// ============================================================
// P9.2 — 获取支持的登录方式列表
// ============================================================
JsonValue OAuthHandler::handle_list_providers(const JsonValue& params)
{
    JsonValue providers = json_array();

    // QQ
    {
        JsonValue qq = json_object();
        qq.object_insert("id",   JsonValue(std::string("qq")));
        qq.object_insert("name", JsonValue(std::string("QQ")));
        qq.object_insert("available", JsonValue(qq_client_ != nullptr));
        if (qq_client_) {
            // 生成一个临时 state (前端应自行生成并存储)
            std::string state = generate_state();
            qq.object_insert("auth_url", JsonValue(qq_client_->build_auth_url(state)));
        }
        providers.array_push_back(qq);
    }

    // 微信
    {
        JsonValue wechat = json_object();
        wechat.object_insert("id",   JsonValue(std::string("wechat")));
        wechat.object_insert("name", JsonValue(std::string("微信")));
        wechat.object_insert("available", JsonValue(wechat_client_ != nullptr));
        if (wechat_client_) {
            std::string state = generate_state();
            wechat.object_insert("auth_url", JsonValue(wechat_client_->build_auth_url(state)));
        }
        providers.array_push_back(wechat);
    }

    // 邮箱
    {
        JsonValue email = json_object();
        email.object_insert("id",   JsonValue(std::string("email")));
        email.object_insert("name", JsonValue(std::string("邮箱")));
        email.object_insert("available", JsonValue(true));
        email.object_insert("has_smtp", JsonValue(email_verifier_ != nullptr));
        providers.array_push_back(email);
    }

    return build_success(providers);
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

    // 验证 OAuth token (调用第三方 API 真实验证)
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

// ============================================================
// OAuth token 验证 — 调用第三方 API 真实验证
// ============================================================
bool OAuthHandler::verify_oauth_token(const std::string& platform,
                                       const std::string& open_id,
                                       const std::string& access_token)
{
    if (access_token.empty()) return false;

    if (platform == "qq") {
        if (!qq_client_) {
            // 未配置 QQ 客户端时，回退到简单格式检查 (兼容模式)
            LOG_WARN("[OAuth] QQ client not configured, using fallback validation");
            return access_token.size() >= 6;
        }
        // 调用 QQ API 真实验证
        bool valid = qq_client_->verify_token(access_token, open_id);
        LOG_INFO("[OAuth] QQ token verify: open_id=%s, result=%s",
                 open_id.c_str(), valid ? "valid" : "invalid");
        return valid;
    }

    if (platform == "wechat") {
        if (!wechat_client_) {
            LOG_WARN("[OAuth] WeChat client not configured, using fallback validation");
            return access_token.size() >= 6;
        }
        bool valid = wechat_client_->verify_token(access_token, open_id);
        LOG_INFO("[OAuth] WeChat token verify: open_id=%s, result=%s",
                 open_id.c_str(), valid ? "valid" : "invalid");
        return valid;
    }

    LOG_WARN("[OAuth] Unknown platform: %s", platform.c_str());
    return false;
}

// ============================================================
// 辅助方法
// ============================================================

std::string OAuthHandler::generate_state()
{
    // 生成 32 位随机十六进制字符串作为 CSRF state
    static const char hex_chars[] = "0123456789abcdef";
    std::string state;
    state.reserve(32);

    // 使用当前时间戳和随机数混合
    uint64_t seed = util::StringUtils::timestamp_ms();
    std::srand(static_cast<unsigned>(seed & 0xFFFFFFFF));

    for (int i = 0; i < 32; i++) {
        state += hex_chars[std::rand() % 16];
    }

    return state;
}

std::string OAuthHandler::generate_code()
{
    // 生成 6 位数字验证码
    uint64_t seed = util::StringUtils::timestamp_ms();
    std::srand(static_cast<unsigned>(seed & 0xFFFFFFFF));

    int code_val = std::rand() % 900000 + 100000; // 100000 ~ 999999

    std::ostringstream oss;
    oss << code_val;
    return oss.str();
}

void OAuthHandler::cleanup_expired_codes()
{
    uint64_t now = util::StringUtils::timestamp_ms();
    for (auto it = email_codes_.begin(); it != email_codes_.end(); ) {
        if (now - it->second.created_at_ms > kCodeExpiryMs) {
            it = email_codes_.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace handler
} // namespace chrono
