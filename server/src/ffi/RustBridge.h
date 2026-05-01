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
     * 是否已初始化
     */
    static bool is_initialized();
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
