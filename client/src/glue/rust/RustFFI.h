#pragma once
/**
 * RustFFI.h — Rust FFI 安全桩
 *
 * 防止 Java UTF-16 ↔ Rust UTF-8 转换时的截断问题。
 *
 * 问题: Java 的 UTF-16 代理对 (surrogate pairs) 在转换为
 * UTF-8 时如果被截断, 会产生无效字节序列。
 *
 * 解决: 所有跨语言字符串通过 Rust 的严格 UTF-8 验证,
 * Rust 端使用 std::str::from_utf8() 拒绝无效序列。
 */
#include "glue/GlueTypes.h"
#include <string>
#include <cstdint>

namespace chrono { namespace glue { namespace rust {

class RustFFI {
public:
    // === 字符串安全验证 ===

    // 验证 UTF-8 字符串, 返回是否合法
    static bool validate_utf8(const std::string& s);

    // 从 UTF-16 转 UTF-8 (完整代理对处理, 不截断)
    // 返回空字符串表示输入无效
    static std::string utf16_to_utf8_safe(const uint16_t* data, size_t len);

    // 从 UTF-8 转 UTF-16 (保证不截断代理对)
    static std::vector<uint16_t> utf8_to_utf16_safe(const std::string& s);

    // === 加密 (Rust 端) ===
    // 由 Rust crypto.rs 提供, 通过 FFI 调用
    // 比 C++ fallback 更安全
    static std::string encrypt_e2e(const std::string& plaintext,
                                    const std::string& key);
    static std::string decrypt_e2e(const std::string& ciphertext,
                                    const std::string& key);
    static std::string generate_key();

    // === 内存安全 ===
    // Rust 端 zeroize 敏感数据, 防止 C++ 端残留
    static void secure_clear(std::string& s);
};

// C FFI 导出 (供 Rust lib 链接)
extern "C" {
    int  rust_validate_utf8(const char* data, size_t len);
    int  rust_utf16_to_utf8(const uint16_t* src, size_t src_len,
                             char* dst, size_t* dst_len);
    int  rust_encrypt(const uint8_t* plain, size_t plain_len,
                      const uint8_t* key, size_t key_len,
                      uint8_t* out, size_t* out_len);
    void rust_secure_clear(uint8_t* data, size_t len);
}

} } } // namespace chrono::glue::rust
