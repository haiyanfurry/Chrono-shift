/**
 * Chrono-shift C++ OAuth 登录处理器
 * QQ / 微信 / 邮箱登录注册
 * C++17 重构版 (P9.2)
 */
#ifndef CHRONO_CPP_OAUTH_HANDLER_H
#define CHRONO_CPP_OAUTH_HANDLER_H

#include "../json/JsonValue.h"
#include "../db/Database.h"
#include "../security/OAuthClient.h"
#include "../security/EmailVerifier.h"
#include <string>
#include <unordered_map>
#include <mutex>

namespace chrono {
namespace handler {

/**
 * OAuth 外部服务配置
 */
struct OAuthConfig {
    security::OAuthClientConfig qq;       // QQ 互联配置
    security::OAuthClientConfig wechat;   // 微信开放平台配置
    security::SmtpConfig        smtp;     // SMTP 邮件配置
    std::string                 base_url; // 服务器公网地址 (用于回调)
};

/**
 * OAuth 第三方登录处理器
 */
class OAuthHandler {
public:
    /**
     * 构造函数 (使用默认空配置，需要后续调用 init_oauth_config 初始化)
     */
    explicit OAuthHandler(db::Database& database);

    /**
     * 构造函数 (带完整 OAuth 配置)
     */
    OAuthHandler(db::Database& database, const OAuthConfig& config);

    /**
     * 初始化/更新 OAuth 配置 (可在运行时动态设置)
     */
    void init_oauth_config(const OAuthConfig& config);

    // ============================================================
    // 原有 API — 保持向后兼容
    // ============================================================

    /**
     * QQ 登录 (已有 access_token + open_id)
     * @param params { "open_id": "...", "access_token": "...", "nickname": "..." }
     */
    json::JsonValue handle_qq_login(const json::JsonValue& params);

    /**
     * 微信登录
     */
    json::JsonValue handle_wechat_login(const json::JsonValue& params);

    /**
     * 邮箱登录 (密码登录)
     */
    json::JsonValue handle_email_login(const json::JsonValue& params);

    /**
     * 邮箱注册 (含验证码)
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

    // ============================================================
    // P9.2 新增 API
    // ============================================================

    /**
     * 获取 QQ 授权 URL
     * @param params { "state": "..." } (可选的 CSRF state)
     * @return { "url": "https://graph.qq.com/oauth2.0/authorize?...", "state": "..." }
     */
    json::JsonValue handle_qq_auth_url(const json::JsonValue& params);

    /**
     * QQ 授权回调处理
     * @param params { "code": "...", "state": "..." }
     *   code: 授权码 (QQ 回调携带)
     *   state: CSRF 状态值
     * @return 登录成功返回用户信息 + JWT token
     */
    json::JsonValue handle_qq_callback(const json::JsonValue& params);

    /**
     * 获取微信授权 URL
     */
    json::JsonValue handle_wechat_auth_url(const json::JsonValue& params);

    /**
     * 微信授权回调处理
     */
    json::JsonValue handle_wechat_callback(const json::JsonValue& params);

    /**
     * 发送邮箱验证码
     * @param params { "email": "..." }
     * @return { "success": true, "message": "验证码已发送" }
     */
    json::JsonValue handle_send_email_code(const json::JsonValue& params);

    /**
     * 获取支持的 OAuth 登录方式列表
     * @return [{ "id": "qq", "name": "QQ", "auth_url": "..." }, ...]
     */
    json::JsonValue handle_list_providers(const json::JsonValue& params);

private:
    db::Database& db_;

    // OAuth 客户端 (可能为 nullptr 如果未配置)
    std::unique_ptr<security::QQClient>     qq_client_;
    std::unique_ptr<security::WechatClient>  wechat_client_;
    std::unique_ptr<security::EmailVerifier> email_verifier_;

    // OAuth 配置
    OAuthConfig config_;
    bool configured_ = false;

    // ============================================================
    // 邮件验证码缓存
    // ============================================================
    struct EmailCodeEntry {
        std::string code;
        uint64_t    created_at_ms; // 创建时间戳 (毫秒)
    };
    std::unordered_map<std::string, EmailCodeEntry> email_codes_;
    std::mutex code_mutex_;
    static constexpr uint64_t kCodeExpiryMs = 5 * 60 * 1000; // 5 分钟有效期

    /**
     * 通用 OAuth 登录逻辑 (由 callback 和旧 login 共用)
     */
    json::JsonValue oauth_login_impl(const std::string& platform,
                                      const json::JsonValue& params);

    /**
     * 验证 OAuth access_token (调用第三方 API)
     */
    bool verify_oauth_token(const std::string& platform,
                             const std::string& open_id,
                             const std::string& access_token);

    /**
     * 生成 CSRF state 值
     */
    std::string generate_state();

    /**
     * 生成 6 位数字验证码
     */
    std::string generate_code();

    /**
     * 清理过期的验证码缓存
     */
    void cleanup_expired_codes();
};

} // namespace handler
} // namespace chrono

#endif // CHRONO_CPP_OAUTH_HANDLER_H
