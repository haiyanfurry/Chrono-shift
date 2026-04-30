//! JWT 令牌生成与验证
//! 提供给 C99 后端的 FFI 接口

use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use std::time::{SystemTime, UNIX_EPOCH};

use jsonwebtoken::{decode, encode, DecodingKey, EncodingKey, Header, Validation};
use serde::{Deserialize, Serialize};

/// JWT 声明结构
#[derive(Debug, Serialize, Deserialize)]
struct Claims {
    sub: String,       // 用户 ID
    exp: usize,        // 过期时间戳
    iat: usize,        // 签发时间戳
    role: String,      // 角色
}

static SECRET: &str = "CHRONO_SHIFT_JWT_SECRET_CHANGE_IN_PRODUCTION";

/// 生成 JWT 令牌
/// 返回令牌字符串，需要调用 rust_free_string 释放
#[no_mangle]
pub extern "C" fn rust_generate_jwt(user_id: *const c_char) -> *mut c_char {
    if user_id.is_null() {
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

    let token = match encode(
        &Header::default(),
        &claims,
        &EncodingKey::from_secret(SECRET.as_ref()),
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
    if token.is_null() {
        return std::ptr::null_mut();
    }
    let t = match unsafe { CStr::from_ptr(token) }.to_str() {
        Ok(s) => s,
        Err(_) => return std::ptr::null_mut(),
    };

    let token_data = match decode::<Claims>(
        t,
        &DecodingKey::from_secret(SECRET.as_ref()),
        &Validation::default(),
    ) {
        Ok(data) => data,
        Err(_) => return std::ptr::null_mut(),
    };

    CString::new(token_data.claims.sub).unwrap_or_default().into_raw()
}
