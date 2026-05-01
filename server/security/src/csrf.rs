//! CSRF 防护模块
//!
//! 对应 C++ [`SecurityManager::CsrfProtector`](server/src/security/SecurityManager.h:140)
//! 提供 Token 生成与常量时间比较验证
//!
//! # 安全设计
//! - Token: 32 字节随机数 → 64 字符 hex 字符串
//! - 验证: 常量时间比较（`constant_time_eq`），防止时序攻击
//! - 无状态：Token 由调用方存储（Session / Cookie）

use rand::Rng;

/// 常量时间比较
///
/// 防止时序侧信道攻击
/// 如果两字符串长度不同，仍然按较长的长度进行比较（不提前返回）
fn constant_time_eq(a: &str, b: &str) -> bool {
    let a_bytes = a.as_bytes();
    let b_bytes = b.as_bytes();
    let max_len = std::cmp::max(a_bytes.len(), b_bytes.len());
    let mut result: u8 = 0;

    for i in 0..max_len {
        let a_byte = a_bytes.get(i).copied().unwrap_or(0);
        let b_byte = b_bytes.get(i).copied().unwrap_or(0);
        result |= a_byte ^ b_byte;
    }

    result == 0
}

/// 生成 CSRF Token
///
/// 32 字节随机数 → 64 字符 hex 字符串
/// 对应 C++ `CsrfProtector::generate_token()`
pub fn generate_token() -> String {
    let mut rng = rand::thread_rng();
    let bytes: [u8; 32] = rng.gen();
    bytes.iter().map(|b| format!("{:02x}", b)).collect()
}

/// 验证 CSRF Token（常量时间比较）
///
/// # 参数
/// - `token`: 用户提交的 token
/// - `stored_token`: 服务端存储的 token
///
/// # 返回
/// - `true`: token 匹配
/// - `false`: token 不匹配
pub fn validate_token(token: &str, stored_token: &str) -> bool {
    constant_time_eq(token, stored_token)
}

// ============================================================
// FFI 导出
// ============================================================

use std::ffi::CString;
use std::os::raw::c_char;

/// C FFI: 生成 CSRF Token
/// @return 分配的 token 字符串，需调用 rust_free_string 释放
#[no_mangle]
pub extern "C" fn rust_csrf_generate_token() -> *mut c_char {
    let token = generate_token();
    let c_str = CString::new(token).unwrap();
    c_str.into_raw()
}

/// C FFI: 验证 CSRF Token（常量时间比较）
/// @param token 用户提交的 token
/// @param stored_token 服务端存储的 token
/// @return 1=匹配, 0=不匹配
#[no_mangle]
pub extern "C" fn rust_csrf_validate_token(
    token: *const c_char,
    stored_token: *const c_char,
) -> i32 {
    if token.is_null() || stored_token.is_null() {
        return 0;
    }
    let tok = match unsafe { std::ffi::CStr::from_ptr(token) }.to_str() {
        Ok(s) => s,
        Err(_) => return 0,
    };
    let stored = match unsafe { std::ffi::CStr::from_ptr(stored_token) }.to_str() {
        Ok(s) => s,
        Err(_) => return 0,
    };
    if validate_token(tok, stored) { 1 } else { 0 }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_generate_token_length() {
        let token = generate_token();
        assert_eq!(token.len(), 64);
    }

    #[test]
    fn test_generate_token_unique() {
        let t1 = generate_token();
        let t2 = generate_token();
        assert_ne!(t1, t2);
    }

    #[test]
    fn test_validate_token_ok() {
        let token = generate_token();
        assert!(validate_token(&token, &token));
    }

    #[test]
    fn test_validate_token_fail() {
        let token = generate_token();
        let other = generate_token();
        assert!(!validate_token(&token, &other));
    }

    #[test]
    fn test_constant_time_eq_same() {
        assert!(constant_time_eq("hello", "hello"));
    }

    #[test]
    fn test_constant_time_eq_diff() {
        assert!(!constant_time_eq("hello", "world"));
    }

    #[test]
    fn test_constant_time_eq_diff_length() {
        assert!(!constant_time_eq("short", "longer_than_short"));
    }

    #[test]
    fn test_constant_time_eq_empty() {
        assert!(constant_time_eq("", ""));
        assert!(!constant_time_eq("", "a"));
    }

    #[test]
    fn test_validate_token_hex_only() {
        let token = generate_token();
        // 只包含 hex 字符
        assert!(token.chars().all(|c| c.is_ascii_hexdigit()));
    }
}
