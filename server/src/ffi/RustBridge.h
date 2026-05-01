/**
 * Chrono-shift C++ Rust FFI 桥接
 * RAII 封装 Rust 安全模块调用
 * C++17 重构版
 */
#ifndef CHRONO_CPP_RUST_BRIDGE_H
#define CHRONO_CPP_RUST_BRIDGE_H

#include <string>
#include <memory>
#include <cstdint>

namespace chrono {
namespace ffi {

// ============================================================
// Rust 外部函数声明 (对应 server/security/src/ 中的函数)
// ============================================================
extern "C" {

/**
 * 初始化 Rust 安全模块
 * @param config_path 配置文件路径
 * @return 0=成功, -1=失败
 */
int rust_server_init(const char* config_path);

/**
 * Argon2id 密码哈希
 * @param password 明文密码
 * @return 分配的哈希字符串指针，需要调用 rust_free_string 释放
 */
char* rust_hash_password(const char* password);

/**
 * Argon2id 密码验证
 * @param password 明文密码
 * @param hash 哈希字符串
 * @return 0=匹配, -1=不匹配, -2=错误
 */
int rust_verify_password(const char* password, const char* hash);

/**
 * 生成 JWT 令牌
 * @param user_id 用户 ID
 * @return 分配的 JWT 字符串指针，需要调用 rust_free_string 释放
 */
char* rust_generate_jwt(const char* user_id);

/**
 * 验证 JWT 令牌
 * @param token JWT 字符串
 * @return 分配的 JSON 字符串 (包含 user_id, exp 等)，需要调用 rust_free_string 释放
 */
char* rust_verify_jwt(const char* token);

/**
 * 释放 Rust 分配的字符串
 * @param s 字符串指针
 */
void rust_free_string(char* s);

/**
 * 获取 Rust 模块版本
 * @return 分配的版本字符串
 */
char* rust_version();

// ============================================================
// RateLimiter (滑动窗口限流)
// ============================================================

/**
 * 初始化限流器
 * @param window_secs 窗口秒数 (0=默认60)
 * @param max_requests 最大请求数 (0=默认100)
 * @return 0=成功
 */
int rust_rate_limiter_init(unsigned long long window_secs, size_t max_requests);

/**
 * 检查是否允许请求
 * @param key 限流 key
 * @return 1=允许, 0=限制
 */
int rust_rate_limiter_allow(const char* key);

/**
 * 清理过期条目
 */
void rust_rate_limiter_cleanup();

/**
 * 重置指定 key 的限流状态
 */
void rust_rate_limiter_reset(const char* key);

// ============================================================
// InputSanitizer (输入净化)
// ============================================================

/**
 * 净化用户名
 * @return 分配的净化后字符串，需 rust_free_string；无效返回 NULL
 */
char* rust_sanitize_username(const char* input);

/**
 * 净化显示名称
 * @return 分配的净化后字符串，需 rust_free_string；无效返回 NULL
 */
char* rust_sanitize_display_name(const char* input);

/**
 * 净化消息 (HTML 转义)
 * @return 分配的转义后字符串，需 rust_free_string
 */
char* rust_sanitize_message(const char* input);

/**
 * 检查密码强度
 * @return 0=Valid, 1=Empty, 2=TooShort, 3=TooLong, 4=MissingUpper, 5=MissingLower, 6=MissingDigit, 7=MissingSpecial
 */
int rust_check_password_strength(const char* password);

/**
 * 验证邮箱格式
 * @return 1=有效, 0=无效
 */
int rust_is_valid_email(const char* email);

/**
 * HTML 实体转义
 * @return 分配的转义字符串，需 rust_free_string
 */
char* rust_escape_html(const char* input);

// ============================================================
// SessionManager (会话管理)
// ============================================================

/**
 * 初始化会话管理器
 * @param timeout_secs 超时秒数 (0=默认3600)
 * @return 0=成功
 */
int rust_session_init(unsigned long long timeout_secs);

/**
 * 创建会话
 * @param user_id 用户ID
 * @param metadata 元数据(可为空)
 * @return 分配的 token 字符串，需 rust_free_string
 */
char* rust_session_create(const char* user_id, const char* metadata);

/**
 * 验证会话
 * @param token 会话 token
 * @return 分配的 user_id 字符串，需 rust_free_string；无效返回 NULL
 */
char* rust_session_validate(const char* token);

/**
 * 移除会话
 */
void rust_session_remove(const char* token);

/**
 * 清理过期会话
 */
void rust_session_cleanup();

/**
 * 刷新会话过期时间
 * @return 1=成功, 0=失败
 */
int rust_session_refresh(const char* token);

/**
 * 获取活跃会话数
 */
int rust_session_active_count();

/**
 * 移除指定用户的所有会话
 */
void rust_session_remove_user(const char* user_id);

// ============================================================
// CSRF (跨站请求伪造防护)
// ============================================================

/**
 * 生成 CSRF Token
 * @return 分配的 token 字符串，需 rust_free_string
 */
char* rust_csrf_generate_token();

/**
 * 验证 CSRF Token (常量时间比较)
 * @param token 用户提交的 token
 * @param stored_token 服务端存储的 token
 * @return 1=匹配, 0=不匹配
 */
int rust_csrf_validate_token(const char* token, const char* stored_token);

// ============================================================
// SafeString (UTF-8 安全校验)
// ============================================================

/**
 * 验证 UTF-8 字符串安全性 (safe_string)
 * 检测: 空字节注入、代理对完整性、编码一致性、控制字符
 * @param input 输入字节指针
 * @param len 输入字节长度
 * @return 0=安全, -1=空字节, -2=孤立代理对, -3=可疑编码, -4=控制字符, -5=非法UTF-8
 */
int rust_validate_utf8_safe(const unsigned char* input, size_t len);

/**
 * 获取校验结果描述
 * @param result rust_validate_utf8_safe 的返回值
 * @return 错误描述字符串，需要调用 rust_free_string 释放
 */
char* rust_validation_result_description(int result);

// ============================================================
// QQ OAuth Client
// ============================================================

/**
 * 初始化 QQ OAuth 客户端
 * @param app_id QQ App ID
 * @param app_key QQ App Key
 * @param redirect_uri 回调地址
 * @return 0=成功, -1=失败
 */
int rust_qq_init(const char* app_id, const char* app_key, const char* redirect_uri);

/**
 * 构建 QQ 授权 URL
 * @param state CSRF 状态码
 * @return 分配的 URL 字符串指针，需要调用 rust_free_string 释放
 */
char* rust_qq_build_auth_url(const char* state);

/**
 * QQ 授权码兑换 (code -> access_token + open_id)
 * @param code 授权码
 * @return 分配的 JSON 结果字符串指针，需要调用 rust_free_string 释放
 */
char* rust_qq_exchange_code(const char* code);

/**
 * 获取 QQ 用户信息
 * @param access_token 访问令牌
 * @param open_id 用户 OpenID
 * @return 分配的 JSON 字符串指针，需要调用 rust_free_string 释放
 */
char* rust_qq_get_user_info(const char* access_token, const char* open_id);

/**
 * 验证 QQ access_token 有效性
 * @param access_token 访问令牌
 * @param open_id 用户 OpenID
 * @return 1=有效, 0=无效
 */
int rust_qq_verify_token(const char* access_token, const char* open_id);

// ============================================================
// WeChat OAuth Client
// ============================================================

/**
 * 初始化微信 OAuth 客户端
 * @param app_id 微信 App ID
 * @param app_key 微信 App Secret
 * @param redirect_uri 回调地址
 * @return 0=成功, -1=失败
 */
int rust_wechat_init(const char* app_id, const char* app_key, const char* redirect_uri);

/**
 * 构建微信授权 URL
 * @param state CSRF 状态码
 * @return 分配的 URL 字符串指针，需要调用 rust_free_string 释放
 */
char* rust_wechat_build_auth_url(const char* state);

/**
 * 微信授权码兑换 (code -> access_token + open_id)
 * @param code 授权码
 * @return 分配的 JSON 结果字符串指针，需要调用 rust_free_string 释放
 */
char* rust_wechat_exchange_code(const char* code);

/**
 * 获取微信用户信息
 * @param access_token 访问令牌
 * @param open_id 用户 OpenID
 * @return 分配的 JSON 字符串指针，需要调用 rust_free_string 释放
 */
char* rust_wechat_get_user_info(const char* access_token, const char* open_id);

/**
 * 验证微信 access_token 有效性
 * @param access_token 访问令牌
 * @param open_id 用户 OpenID
 * @return 1=有效, 0=无效
 */
int rust_wechat_verify_token(const char* access_token, const char* open_id);

// ============================================================
// Email Verifier (SMTP)
// ============================================================

/**
 * 初始化邮箱验证器 (SMTP)
 * @param host SMTP 服务器地址
 * @param port SMTP 端口
 * @param username SMTP 用户名
 * @param password SMTP 密码
 * @param from_addr 发件人地址
 * @param from_name 发件人名称 (可为空)
 * @param use_tls 是否使用 TLS (1=是, 0=否)
 * @return 0=成功, -1=失败
 */
int rust_email_init(const char* host, int port, const char* username, const char* password, const char* from_addr, const char* from_name, int use_tls);

/**
 * 发送验证码邮件
 * @param to_email 收件人邮箱
 * @param code 验证码
 * @return 0=成功, -1=失败
 */
int rust_email_send_code(const char* to_email, const char* code);

/**
 * 清理邮箱验证器
 */
void rust_email_cleanup();

} // extern "C"

/**
 * Rust 安全模块 RAII 包装器
 * 自动管理 Rust 字符串的生命周期
 */
class RustBridge {
public:
    /**
     * 初始化 Rust 安全模块
     * @param config_path 配置文件路径
     */
    static bool init(const std::string& config_path);

    /**
     * 密码哈希 (RAII 自动管理内存)
     */
    static std::string hash_password(const std::string& password);

    /**
     * 密码验证
     * @return true 如果密码匹配
     */
    static bool verify_password(const std::string& password, const std::string& hash);

    /**
     * 生成 JWT
     */
    static std::string generate_jwt(const std::string& user_id);

    /**
     * 验证 JWT
     * @return JSON 字符串，包含 user_id; 失败返回空字符串
     */
    static std::string verify_jwt(const std::string& token);

    /**
     * 获取版本
     */
    static std::string version();

    /**
     * 验证 UTF-8 字符串安全性 (safe_string)
     * 调用 Rust safe_string 模块进行深度字符串安全校验
     * @param input 待校验的字符串
     * @return 0=安全, 负数=错误码 (同 ValidationResult)
     */
    static int validate_string(const std::string& input);

    /**
     * 获取校验结果描述
     * @param result validate_string 的返回值
     * @return 人类可读的错误描述
     */
    static std::string validation_result_description(int result);

    /**
     * 是否已初始化
     */
    static bool is_initialized();

    // ============================================================
    // RateLimiter
    // ============================================================

    /**
     * 初始化限流器
     */
    static bool rate_limiter_init(unsigned long long window_secs = 60, size_t max_requests = 100);

    /**
     * 检查是否允许请求
     */
    static bool rate_limiter_allow(const std::string& key);

    /**
     * 清理过期条目
     */
    static void rate_limiter_cleanup();

    /**
     * 重置指定 key 的限流状态
     */
    static void rate_limiter_reset(const std::string& key);

    // ============================================================
    // InputSanitizer
    // ============================================================

    /**
     * 净化用户名
     * @return 净化后的字符串，空字符串表示无效
     */
    static std::string sanitize_username(const std::string& input);

    /**
     * 净化显示名称
     * @return 净化后的字符串，空字符串表示无效
     */
    static std::string sanitize_display_name(const std::string& input);

    /**
     * 净化消息 (HTML 转义)
     */
    static std::string sanitize_message(const std::string& input);

    /**
     * 检查密码强度
     * @return 0=Valid, 负数=错误码
     */
    static int check_password_strength(const std::string& password);

    /**
     * 验证邮箱格式
     */
    static bool is_valid_email(const std::string& email);

    /**
     * HTML 实体转义
     */
    static std::string escape_html(const std::string& input);

    // ============================================================
    // SessionManager
    // ============================================================

    /**
     * 初始化会话管理器
     */
    static bool session_init(unsigned long long timeout_secs = 3600);

    /**
     * 创建会话
     * @return 会话 token，空字符串表示失败
     */
    static std::string session_create(const std::string& user_id, const std::string& metadata = "");

    /**
     * 验证会话
     * @return user_id，空字符串表示无效
     */
    static std::string session_validate(const std::string& token);

    /**
     * 移除会话
     */
    static void session_remove(const std::string& token);

    /**
     * 清理过期会话
     */
    static void session_cleanup();

    /**
     * 刷新会话过期时间
     */
    static bool session_refresh(const std::string& token);

    /**
     * 获取活跃会话数
     */
    static int session_active_count();

    /**
     * 移除指定用户的所有会话
     */
    static void session_remove_user(const std::string& user_id);

    // ============================================================
    // CSRF
    // ============================================================

    /**
     * 生成 CSRF Token
     */
    static std::string csrf_generate_token();

    /**
     * 验证 CSRF Token (常量时间比较)
     */
    static bool csrf_validate_token(const std::string& token, const std::string& stored_token);

    // ============================================================
    // QQ OAuth Client
    // ============================================================

    /**
     * 初始化 QQ OAuth 客户端
     */
    static bool qq_init(const std::string& app_id, const std::string& app_key, const std::string& redirect_uri);

    /**
     * 构建 QQ 授权 URL
     * @return 授权 URL，空字符串表示失败
     */
    static std::string qq_build_auth_url(const std::string& state);

    /**
     * QQ 授权码兑换
     * @return JSON 字符串 (包含 access_token, open_id)，空字符串表示失败
     */
    static std::string qq_exchange_code(const std::string& code);

    /**
     * 获取 QQ 用户信息
     * @return JSON 字符串，空字符串表示失败
     */
    static std::string qq_get_user_info(const std::string& access_token, const std::string& open_id);

    /**
     * 验证 QQ access_token
     */
    static bool qq_verify_token(const std::string& access_token, const std::string& open_id);

    // ============================================================
    // WeChat OAuth Client
    // ============================================================

    /**
     * 初始化微信 OAuth 客户端
     */
    static bool wechat_init(const std::string& app_id, const std::string& app_key, const std::string& redirect_uri);

    /**
     * 构建微信授权 URL
     * @return 授权 URL，空字符串表示失败
     */
    static std::string wechat_build_auth_url(const std::string& state);

    /**
     * 微信授权码兑换
     * @return JSON 字符串 (包含 access_token, open_id)，空字符串表示失败
     */
    static std::string wechat_exchange_code(const std::string& code);

    /**
     * 获取微信用户信息
     * @return JSON 字符串，空字符串表示失败
     */
    static std::string wechat_get_user_info(const std::string& access_token, const std::string& open_id);

    /**
     * 验证微信 access_token
     */
    static bool wechat_verify_token(const std::string& access_token, const std::string& open_id);

    // ============================================================
    // Email Verifier (SMTP)
    // ============================================================

    /**
     * 初始化邮箱验证器
     * @param use_tls 是否使用 TLS
     */
    static bool email_init(const std::string& host, int port, const std::string& username, const std::string& password, const std::string& from_addr, const std::string& from_name = "", bool use_tls = false);

    /**
     * 发送验证码
     */
    static bool email_send_code(const std::string& to_email, const std::string& code);

    /**
     * 清理邮箱验证器
     */
    static void email_cleanup(void);
};

/**
 * RAII 包装器，自动释放 Rust 分配的字符串
 */
class RustString {
public:
    explicit RustString(char* ptr);
    ~RustString();

    // 禁止拷贝
    RustString(const RustString&) = delete;
    RustString& operator=(const RustString&) = delete;

    // 允许移动
    RustString(RustString&& other) noexcept;
    RustString& operator=(RustString&& other) noexcept;

    const char* get() const { return ptr_; }
    std::string str() const { return ptr_ ? std::string(ptr_) : ""; }
    bool is_null() const { return ptr_ == nullptr; }

private:
    char* ptr_ = nullptr;
};

} // namespace ffi
} // namespace chrono

#endif // CHRONO_CPP_RUST_BRIDGE_H
