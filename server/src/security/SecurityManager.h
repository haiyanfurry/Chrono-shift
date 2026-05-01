/**
 * Chrono-shift C++ 安全模块
 * 安全管理器 - 统筹认证、速率限制、输入消毒等
 * C++17 重构版
 */
#ifndef CHRONO_CPP_SECURITY_MANAGER_H
#define CHRONO_CPP_SECURITY_MANAGER_H

#include <string>
#include <unordered_map>
#include <chrono>
#include <vector>
#include <mutex>
#include <functional>

namespace chrono {
namespace security {

/**
 * 速率限制器 (Token Bucket)
 */
class RateLimiter {
public:
    /**
     * @param max_requests 时间窗口内最大请求数
     * @param window_ms    时间窗口 (毫秒)
     */
    RateLimiter(size_t max_requests, size_t window_ms);

    /**
     * 检查是否允许请求
     * @param key 标识 (如 IP 地址)
     * @return true 如果允许
     */
    bool allow(const std::string& key);

    /**
     * 清理过期条目
     */
    void cleanup();

private:
    struct Entry {
        size_t count;
        std::chrono::steady_clock::time_point window_start;
    };

    size_t max_requests_;
    size_t window_ms_;
    std::unordered_map<std::string, Entry> entries_;
    std::mutex mutex_;
};

/**
 * 输入消毒器
 */
class InputSanitizer {
public:
    /**
     * 消毒用户名: 只允许字母、数字、下划线、连字符
     */
    static std::string sanitize_username(const std::string& input);

    /**
     * 消毒显示名称
     */
    static std::string sanitize_display_name(const std::string& input);

    /**
     * 消毒消息内容 (防止 XSS)
     */
    static std::string sanitize_message(const std::string& input);

    /**
     * 检查密码强度
     * @return 空字符串表示通过，否则返回错误描述
     */
    static std::string check_password_strength(const std::string& password);

    /**
     * 验证邮箱格式
     */
    static bool is_valid_email(const std::string& email);

    /**
     * HTML 转义
     */
    static std::string escape_html(const std::string& input);
};

/**
 * 会话管理器
 */
class SessionManager {
public:
    SessionManager() = default;

    /**
     * 创建会话
     * @param user_id 用户 ID
     * @param duration_ms 会话持续时间 (毫秒)
     * @return 会话令牌
     */
    std::string create_session(const std::string& user_id,
                                uint64_t duration_ms = 3600000); // 默认 1 小时

    /**
     * 验证会话
     * @return 用户 ID，无效返回空字符串
     */
    std::string validate_session(const std::string& token);

    /**
     * 销毁会话
     */
    void destroy_session(const std::string& token);

    /**
     * 清理过期会话
     */
    void cleanup();

private:
    struct Session {
        std::string user_id;
        std::chrono::steady_clock::time_point expires_at;
    };

    std::unordered_map<std::string, Session> sessions_;
    std::mutex mutex_;
};

/**
 * CSRF 防护
 */
class CsrfProtector {
public:
    /**
     * 生成 CSRF 令牌
     */
    static std::string generate_token();

    /**
     * 验证 CSRF 令牌
     */
    static bool validate_token(const std::string& token, const std::string& expected);
};

} // namespace security
} // namespace chrono

#endif // CHRONO_CPP_SECURITY_MANAGER_H
