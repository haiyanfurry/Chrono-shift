/**
 * Chrono-shift 会话管理
 * C++17 重构版
 * 管理登录令牌、会话状态，通过 Rust FFI 与底层安全存储交互
 */
#ifndef CHRONO_CLIENT_SESSION_MANAGER_H
#define CHRONO_CLIENT_SESSION_MANAGER_H

#include <string>

namespace chrono {
namespace client {
namespace storage {

/**
 * 会话状态
 */
struct SessionState {
    std::string user_id;       ///< 用户 ID
    std::string username;      ///< 用户名
    std::string token;         ///< 认证令牌
    bool        logged_in = false; ///< 是否已登录
};

/**
 * 会话管理器
 *
 * 在 C++ 层提供会话管理接口。
 * 底层存储通过 Rust FFI (client/security/src/session.rs) 实现。
 * 如果 Rust 库不可用，则退化为纯内存模式。
 */
class SessionManager {
public:
    SessionManager();
    ~SessionManager();

    SessionManager(const SessionManager&) = delete;
    SessionManager& operator=(const SessionManager&) = delete;
    SessionManager(SessionManager&&) = default;
    SessionManager& operator=(SessionManager&&) = default;

    /**
     * 初始化会话管理器
     * @return 0 成功, -1 失败
     */
    int init();

    /**
     * 保存会话
     * @param user_id  用户 ID
     * @param username 用户名
     * @param token    认证令牌
     * @return 0 成功, -1 失败
     */
    int save(const std::string& user_id, const std::string& username,
             const std::string& token);

    /** 获取认证令牌 */
    std::string get_token() const;

    /** 检查是否已登录 */
    bool is_logged_in() const;

    /** 获取用户 ID */
    std::string get_user_id() const;

    /** 获取用户名 */
    std::string get_username() const;

    /** 清除会话 */
    void clear();

    /** 获取当前会话状态 */
    SessionState get_state() const;

private:
    SessionState state_;
};

} // namespace storage
} // namespace client
} // namespace chrono

#endif // CHRONO_CLIENT_SESSION_MANAGER_H
