/**
 * Chrono-shift 令牌管理器
 * C++17 重构版
 *
 * 管理 JWT 认证令牌的获取、刷新、缓存。
 */
#ifndef CHRONO_CLIENT_TOKEN_MANAGER_H
#define CHRONO_CLIENT_TOKEN_MANAGER_H

#include <string>
#include <chrono>

namespace chrono {
namespace client {
namespace security {

/**
 * 令牌管理器
 */
class TokenManager {
public:
    TokenManager();
    ~TokenManager();

    TokenManager(const TokenManager&) = delete;
    TokenManager& operator=(const TokenManager&) = delete;
    TokenManager(TokenManager&&) = default;
    TokenManager& operator=(TokenManager&&) = default;

    /**
     * 初始化令牌管理器
     * @return 0 成功, -1 失败
     */
    int init();

    /**
     * 设置访问令牌
     * @param token        JWT 令牌
     * @param expires_in   过期时间 (秒, 0=不过期)
     */
    void set_access_token(const std::string& token, uint32_t expires_in = 3600);

    /**
     * 设置刷新令牌
     * @param token 刷新令牌
     */
    void set_refresh_token(const std::string& token);

    /** 获取访问令牌 */
    std::string get_access_token() const;

    /** 获取刷新令牌 */
    std::string get_refresh_token() const;

    /**
     * 检查令牌是否有效 (未过期)
     * @return true 有效, false 无效
     */
    bool is_token_valid() const;

    /**
     * 获取令牌过期时间戳 (Unix 纪元秒)
     * @return 过期时间戳, 0 表示不过期
     */
    uint64_t expires_at() const { return expires_at_; }

    /** 清除令牌 */
    void clear();

private:
    std::string access_token_;
    std::string refresh_token_;
    uint64_t expires_at_ = 0; ///< Unix 纪元秒
};

} // namespace security
} // namespace client
} // namespace chrono

#endif // CHRONO_CLIENT_TOKEN_MANAGER_H
