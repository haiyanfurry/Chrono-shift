/**
 * Chrono-shift C++ OAuth 登录处理器
 * QQ / 微信 / 邮箱登录注册
 * C++17 重构版 (P9.2)
 */
#ifndef CHRONO_CPP_OAUTH_HANDLER_H
#define CHRONO_CPP_OAUTH_HANDLER_H

#include "../json/JsonValue.h"
#include "../db/Database.h"
#include <string>

namespace chrono {
namespace handler {

/**
 * OAuth 第三方登录处理器
 */
class OAuthHandler {
public:
    explicit OAuthHandler(db::Database& database);

    /**
     * QQ 登录
     * @param params { "open_id": "...", "access_token": "...", "nickname": "..." }
     */
    json::JsonValue handle_qq_login(const json::JsonValue& params);

    /**
     * 微信登录
     * @param params { "open_id": "...", "access_token": "...", "nickname": "..." }
     */
    json::JsonValue handle_wechat_login(const json::JsonValue& params);

    /**
     * 邮箱登录 (密码登录)
     * @param params { "email": "...", "password": "..." }
     */
    json::JsonValue handle_email_login(const json::JsonValue& params);

    /**
     * 邮箱注册
     * @param params { "email": "...", "password": "...", "code": "...", "username": "..." }
     */
    json::JsonValue handle_email_register(const json::JsonValue& params);

    /**
     * 绑定 OAuth 账号到已有用户
     */
    json::JsonValue handle_bind_oauth(const json::JsonValue& params);

    /**
     * 解绑 OAuth 账号
     */
    json::JsonValue handle_unbind_oauth(const json::JsonValue& params);

private:
    db::Database& db_;

    /**
     * 通用 OAuth 登录逻辑
     */
    json::JsonValue oauth_login_impl(const std::string& platform,
                                      const json::JsonValue& params);

    /**
     * 验证 OAuth access_token (调用第三方 API)
     * 简化版：跳过真实验证，仅检查格式
     */
    bool verify_oauth_token(const std::string& platform,
                             const std::string& open_id,
                             const std::string& access_token);
};

} // namespace handler
} // namespace chrono

#endif // CHRONO_CPP_OAUTH_HANDLER_H
