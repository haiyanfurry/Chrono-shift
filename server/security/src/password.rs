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

// ============================================================
// 单元测试
// ============================================================
#[cfg(test)]
mod tests {
    use super::*;
    use std::ffi::CString;

    #[test]
    fn test_hash_password_valid() {
        let pwd = CString::new("TestPassword123!@#").unwrap();
        let hash_ptr = rust_hash_password(pwd.as_ptr());
        assert!(!hash_ptr.is_null());

        let hash_str = unsafe { CStr::from_ptr(hash_ptr) }.to_str().unwrap().to_string();
        unsafe { CString::from_raw(hash_ptr) }; // free

        // Argon2id 哈希以 "$argon2id$" 开头
        assert!(hash_str.starts_with("$argon2id$"));
        assert!(hash_str.len() > 50);
    }

    #[test]
    fn test_hash_password_null() {
        let hash_ptr = rust_hash_password(std::ptr::null());
        assert!(hash_ptr.is_null());
    }

    #[test]
    fn test_hash_password_empty() {
        let pwd = CString::new("").unwrap();
        let hash_ptr = rust_hash_password(pwd.as_ptr());
        assert!(!hash_ptr.is_null());
        let hash_str = unsafe { CStr::from_ptr(hash_ptr) }.to_str().unwrap().to_string();
        unsafe { CString::from_raw(hash_ptr) };
        assert!(hash_str.starts_with("$argon2id$"));
    }

    #[test]
    fn test_verify_password_correct() {
        let pwd = CString::new("MySecureP@ssw0rd").unwrap();
        let hash_ptr = rust_hash_password(pwd.as_ptr());
        assert!(!hash_ptr.is_null());

        let result = rust_verify_password(pwd.as_ptr(), hash_ptr);
        assert_eq!(result, 1);

        unsafe { CString::from_raw(hash_ptr) };
    }

    #[test]
    fn test_verify_password_wrong() {
        let pwd = CString::new("CorrectPassword").unwrap();
        let wrong = CString::new("WrongPassword").unwrap();
        let hash_ptr = rust_hash_password(pwd.as_ptr());
        assert!(!hash_ptr.is_null());

        let result = rust_verify_password(wrong.as_ptr(), hash_ptr);
        assert_eq!(result, 0);

        unsafe { CString::from_raw(hash_ptr) };
    }

    #[test]
    fn test_verify_password_null() {
        let pwd = CString::new("test").unwrap();
        let hash_ptr = rust_hash_password(pwd.as_ptr());
        assert!(!hash_ptr.is_null());

        let result = rust_verify_password(std::ptr::null(), hash_ptr);
        assert_eq!(result, -1);

        let result2 = rust_verify_password(pwd.as_ptr(), std::ptr::null());
        assert_eq!(result2, -1);

        unsafe { CString::from_raw(hash_ptr) };
    }

    #[test]
    fn test_verify_password_invalid_hash() {
        let pwd = CString::new("test").unwrap();
        let invalid_hash = CString::new("$invalid$hash$format").unwrap();

        let result = rust_verify_password(pwd.as_ptr(), invalid_hash.as_ptr());
        assert_eq!(result, -1);
    }

    #[test]
    fn test_hash_is_unique() {
        // 相同密码每次哈希结果不同 (因为盐值)
        let pwd = CString::new("SamePassword").unwrap();
        let hash1_ptr = rust_hash_password(pwd.as_ptr());
        let hash2_ptr = rust_hash_password(pwd.as_ptr());
        assert!(!hash1_ptr.is_null());
        assert!(!hash2_ptr.is_null());

        let hash1 = unsafe { CStr::from_ptr(hash1_ptr) }.to_str().unwrap().to_string();
        let hash2 = unsafe { CStr::from_ptr(hash2_ptr) }.to_str().unwrap().to_string();

        assert_ne!(hash1, hash2, "相同密码的两次哈希应该不同（不同盐值）");

        unsafe { CString::from_raw(hash1_ptr) };
        unsafe { CString::from_raw(hash2_ptr) };
    }

    #[test]
    fn test_verify_special_characters() {
        let special = CString::new("密码123!@#$%^&*()_+-=[]{}|;':\",./<>?~`").unwrap();
        let hash_ptr = rust_hash_password(special.as_ptr());
        assert!(!hash_ptr.is_null());

        let result = rust_verify_password(special.as_ptr(), hash_ptr);
        assert_eq!(result, 1, "特殊字符密码验证应成功");

        unsafe { CString::from_raw(hash_ptr) };
    }
}
