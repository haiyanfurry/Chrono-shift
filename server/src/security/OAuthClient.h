/**
 * Chrono-shift C++ OAuth HTTP 客户端
 * QQ / 微信 OAuth2.0 授权码流程
 * C++17 重构版 (P9.2)
 */
#ifndef CHRONO_CPP_OAUTH_CLIENT_H
#define CHRONO_CPP_OAUTH_CLIENT_H

#include <string>
#include <cstdint>

namespace chrono {
namespace security {

/**
 * OAuth 客户端配置
 */
struct OAuthClientConfig {
    std::string app_id;       // QQ App ID / 微信 AppID
    std::string app_key;      // QQ App Key / 微信 AppSecret
    std::string redirect_uri; // 回调地址 (需 URL 编码)
};

/**
 * OAuth 用户信息
 */
struct OAuthUserInfo {
    std::string open_id;
    std::string nickname;
    std::string avatar_url;
};

// ============================================================
// QQ 登录客户端
// ============================================================

class QQClient {
public:
    explicit QQClient(OAuthClientConfig config);

    /**
     * 构建 QQ 授权 URL
     * @param state CSRF 状态值
     * @return 完整的授权页 URL
     */
    std::string build_auth_url(const std::string& state) const;

    /**
     * 用授权码换取 access_token 和 open_id
     * @param code 授权码
     * @param[out] access_token 返回的 access_token
     * @param[out] open_id 返回的 open_id
     * @return true 表示成功
     */
    bool exchange_code(const std::string& code,
                       std::string& access_token,
                       std::string& open_id);

    /**
     * 获取 QQ 用户信息
     * @param access_token 有效的 access_token
     * @param open_id 用户的 open_id
     * @param[out] info 返回的用户信息
     * @return true 表示成功
     */
    bool get_user_info(const std::string& access_token,
                       const std::string& open_id,
                       OAuthUserInfo& info);

    /**
     * 验证 access_token 是否有效
     * @param access_token 待验证的 token
     * @param open_id 用户的 open_id
     * @return true 表示有效
     */
    bool verify_token(const std::string& access_token,
                      const std::string& open_id);

private:
    OAuthClientConfig config_;

    /**
     * HTTP GET 请求
     * @param url 完整 URL
     * @param[out] response_body 响应体
     * @return true 表示成功
     */
    bool http_get(const std::string& url, std::string& response_body);

    /**
     * 解析 URL 编码的键值对响应 (如 access_token=xxx&expires_in=xxx)
     */
    static std::string parse_url_param(const std::string& body,
                                        const std::string& key);
};

// ============================================================
// 微信登录客户端
// ============================================================

class WechatClient {
public:
    explicit WechatClient(OAuthClientConfig config);

    /**
     * 构建微信授权 URL
     */
    std::string build_auth_url(const std::string& state) const;

    /**
     * 用授权码换取 access_token 和 open_id
     */
    bool exchange_code(const std::string& code,
                       std::string& access_token,
                       std::string& open_id);

    /**
     * 获取微信用户信息
     */
    bool get_user_info(const std::string& access_token,
                       const std::string& open_id,
                       OAuthUserInfo& info);

    /**
     * 验证 access_token
     */
    bool verify_token(const std::string& access_token,
                      const std::string& open_id);

private:
    OAuthClientConfig config_;

    bool http_get(const std::string& url, std::string& response_body);
};

} // namespace security
} // namespace chrono

#endif // CHRONO_CPP_OAUTH_CLIENT_H
