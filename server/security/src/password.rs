//! 密码哈希与验证 (Argon2id)
//! 提供给 C99 后端的 FFI 接口

use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use argon2::{Argon2, PasswordHash, PasswordHasher, PasswordVerifier};
use argon2::password_hash::SaltString;
use rand::rngs::OsRng;

/// 使用 Argon2id 对密码进行哈希
/// 返回哈希字符串，需要调用 rust_free_string 释放
#[no_mangle]
pub extern "C" fn rust_hash_password(password: *const c_char) -> *mut c_char {
    if password.is_null() {
        return std::ptr::null_mut();
    }
    let pwd = match unsafe { CStr::from_ptr(password) }.to_str() {
        Ok(s) => s,
        Err(_) => return std::ptr::null_mut(),
    };

    let salt = SaltString::generate(&mut OsRng);
    let argon2 = Argon2::default();
    let hash = match argon2.hash_password(pwd.as_bytes(), &salt) {
        Ok(h) => h.to_string(),
        Err(_) => return std::ptr::null_mut(),
    };

    CString::new(hash).unwrap_or_default().into_raw()
}

/// 验证密码是否匹配哈希
/// 返回 1 匹配，0 不匹配，-1 错误
#[no_mangle]
pub extern "C" fn rust_verify_password(
    password: *const c_char,
    hash: *const c_char,
) -> i32 {
    if password.is_null() || hash.is_null() {
        return -1;
    }
    let pwd = match unsafe { CStr::from_ptr(password) }.to_str() {
        Ok(s) => s,
        Err(_) => return -1,
    };
    let hash_str = match unsafe { CStr::from_ptr(hash) }.to_str() {
        Ok(s) => s,
        Err(_) => return -1,
    };

    let parsed_hash = match PasswordHash::new(hash_str) {
        Ok(h) => h,
        Err(_) => return -1,
    };

    match Argon2::default().verify_password(pwd.as_bytes(), &parsed_hash) {
        Ok(_) => 1,
        Err(_) => 0,
    }
}
