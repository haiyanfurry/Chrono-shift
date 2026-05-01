//! JWT 令牌生成与验证
//! 提供给 C99 后端的 FFI 接口
//!
//! 安全性: 签名密钥通过 key_mgmt 模块从磁盘加载随机生成的密钥,
//! 不再使用编译期硬编码的静态字符串。

use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use std::sync::OnceLock;
use std::time::{SystemTime, UNIX_EPOCH};

use jsonwebtoken::{decode, encode, DecodingKey, EncodingKey, Header, Validation};
use serde::{Deserialize, Serialize};

use crate::key_mgmt;

/// JWT 声明结构
#[derive(Debug, Serialize, Deserialize)]
struct Claims {
    sub: String,       // 用户 ID
    exp: usize,        // 过期时间戳
    iat: usize,        // 签发时间戳
    role: String,      // 角色
}

/// 缓存 JWT 密钥加载结果
static JWT_KEY: OnceLock<[u8; 32]> = OnceLock::new();

/// 初始化 JWT 子系统 (从 key_mgmt 加载密钥)
/// 在 rust_server_init 之后调用
fn ensure_key_loaded() -> bool {
    JWT_KEY.get().is_some() || {
        if let Some(key) = key_mgmt::get_jwt_secret() {
            let _ = JWT_KEY.set(key);
            true
        } else {
            false
        }
    }
}

/// 生成 JWT 令牌
/// 返回令牌字符串，需要调用 rust_free_string 释放
#[no_mangle]
pub extern "C" fn rust_generate_jwt(user_id: *const c_char) -> *mut c_char {
    if user_id.is_null() || !ensure_key_loaded() {
        return std::ptr::null_mut();
    }
    let uid = match unsafe { CStr::from_ptr(user_id) }.to_str() {
        Ok(s) => s,
        Err(_) => return std::ptr::null_mut(),
    };

    let now = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap_or_default()
        .as_secs() as usize;

    let claims = Claims {
        sub: uid.to_string(),
        exp: now + 86400 * 7, // 7 天过期
        iat: now,
        role: "user".to_string(),
    };

    let secret = JWT_KEY.get().unwrap();
    let token = match encode(
        &Header::default(),
        &claims,
        &EncodingKey::from_secret(secret.as_ref()),
    ) {
        Ok(t) => t,
        Err(_) => return std::ptr::null_mut(),
    };

    CString::new(token).unwrap_or_default().into_raw()
}

/// 验证 JWT 令牌
/// 成功返回用户 ID 字符串，失败返回 NULL
#[no_mangle]
pub extern "C" fn rust_verify_jwt(token: *const c_char) -> *mut c_char {
    if token.is_null() || !ensure_key_loaded() {
        return std::ptr::null_mut();
    }
    let t = match unsafe { CStr::from_ptr(token) }.to_str() {
        Ok(s) => s,
        Err(_) => return std::ptr::null_mut(),
    };

    let secret = JWT_KEY.get().unwrap();
    let token_data = match decode::<Claims>(
        t,
        &DecodingKey::from_secret(secret.as_ref()),
        &Validation::default(),
    ) {
        Ok(data) => data,
        Err(_) => return std::ptr::null_mut(),
    };

    CString::new(token_data.claims.sub).unwrap_or_default().into_raw()
}

// ============================================================
// 单元测试
// ============================================================
#[cfg(test)]
mod tests {
    use super::*;
    use std::ffi::CString;

    #[test]
    fn test_generate_jwt_valid() {
        let uid = CString::new("user_42").unwrap();
        let token_ptr = rust_generate_jwt(uid.as_ptr());
        assert!(!token_ptr.is_null());

        let token = unsafe { CStr::from_ptr(token_ptr) }.to_str().unwrap().to_string();
        unsafe { CString::from_raw(token_ptr) };

        // JWT 格式: header.payload.signature (3 部分)
        let parts: Vec<&str> = token.split('.').collect();
        assert_eq!(parts.len(), 3, "JWT 应有 3 部分");
        assert!(!parts[0].is_empty());
        assert!(!parts[1].is_empty());
        assert!(!parts[2].is_empty());
    }

    #[test]
    fn test_generate_jwt_null() {
        let token_ptr = rust_generate_jwt(std::ptr::null());
        assert!(token_ptr.is_null());
    }

    #[test]
    fn test_generate_jwt_empty_id() {
        let uid = CString::new("").unwrap();
        let token_ptr = rust_generate_jwt(uid.as_ptr());
        assert!(!token_ptr.is_null());
        unsafe { CString::from_raw(token_ptr) };
    }

    #[test]
    fn test_verify_jwt_valid() {
        let uid = CString::new("user_99").unwrap();
        let token_ptr = rust_generate_jwt(uid.as_ptr());
        assert!(!token_ptr.is_null());

        let extracted_uid_ptr = rust_verify_jwt(token_ptr);
        assert!(!extracted_uid_ptr.is_null());

        let extracted = unsafe { CStr::from_ptr(extracted_uid_ptr) }.to_str().unwrap().to_string();
        assert_eq!(extracted, "user_99", "JWT 验证应提取正确的用户 ID");

        unsafe { CString::from_raw(token_ptr) };
        unsafe { CString::from_raw(extracted_uid_ptr) };
    }

    #[test]
    fn test_verify_jwt_invalid_token() {
        let invalid = CString::new("invalid.jwt.token").unwrap();
        let result = rust_verify_jwt(invalid.as_ptr());
        assert!(result.is_null(), "无效 JWT 应返回 NULL");
    }

    #[test]
    fn test_verify_jwt_tampered() {
        let uid = CString::new("user_1").unwrap();
        let token_ptr = rust_generate_jwt(uid.as_ptr());
        assert!(!token_ptr.is_null());

        let mut token = unsafe { CStr::from_ptr(token_ptr) }.to_str().unwrap().to_string();
        unsafe { CString::from_raw(token_ptr) };

        // 篡改 payload 部分
        token.push_str("x");

        let tampered = CString::new(token).unwrap();
        let result = rust_verify_jwt(tampered.as_ptr());
        assert!(result.is_null(), "被篡改的 JWT 应验证失败");
    }

    #[test]
    fn test_verify_jwt_null() {
        let result = rust_verify_jwt(std::ptr::null());
        assert!(result.is_null());
    }

    #[test]
    fn test_generate_multiple_tokens() {
        let uid = CString::new("user_1").unwrap();
        let token1_ptr = rust_generate_jwt(uid.as_ptr());
        let token2_ptr = rust_generate_jwt(uid.as_ptr());
        assert!(!token1_ptr.is_null());
        assert!(!token2_ptr.is_null());

        let token1 = unsafe { CStr::from_ptr(token1_ptr) }.to_str().unwrap().to_string();
        let token2 = unsafe { CStr::from_ptr(token2_ptr) }.to_str().unwrap().to_string();

        assert_ne!(token1, token2);

        unsafe { CString::from_raw(token1_ptr) };
        unsafe { CString::from_raw(token2_ptr) };
    }

    #[test]
    fn test_verify_jwt_expired() {
        // 确保密钥已加载（从 key_mgmt 获取运行时密钥）
        ensure_key_loaded();

        // 手动构造一个已过期的 JWT
        let expired_claims = Claims {
            sub: "expired_user".to_string(),
            exp: 1000000000,
            iat: 1000000000,
            role: "user".to_string(),
        };

        let secret = JWT_KEY.get().expect("JWT_KEY 应在 ensure_key_loaded 后初始化");
        let token = encode(
            &Header::default(),
            &expired_claims,
            &EncodingKey::from_secret(secret.as_ref()),
        ).unwrap();

        let token_cstr = CString::new(token).unwrap();
        let result = rust_verify_jwt(token_cstr.as_ptr());
        assert!(result.is_null(), "过期 JWT 应验证失败");
    }
}
