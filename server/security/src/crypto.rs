//! 消息加密/解密 (AES-256-GCM)
//! 提供给 C99 后端的 FFI 接口

use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use std::ptr;

use aes_gcm::{Aes256Gcm, Key, Nonce};
use aes_gcm::aead::{Aead, AeadCore, KeyInit, OsRng};
use base64::{Engine as _, engine::general_purpose::STANDARD as BASE64};

/// 使用 AES-256-GCM 加密消息
/// 返回 base64 编码的密文字符串，需要调用 rust_free_string 释放
#[no_mangle]
pub extern "C" fn rust_encrypt_message(
    plaintext: *const c_char,
    key_base64: *const c_char,
) -> *mut c_char {
    if plaintext.is_null() || key_base64.is_null() {
        return ptr::null_mut();
    }

    let text = match unsafe { CStr::from_ptr(plaintext) }.to_str() {
        Ok(s) => s,
        Err(_) => return ptr::null_mut(),
    };
    let key_str = match unsafe { CStr::from_ptr(key_base64) }.to_str() {
        Ok(s) => s,
        Err(_) => return ptr::null_mut(),
    };

    // 解码密钥
    let key_bytes = match BASE64.decode(key_str) {
        Ok(b) => b,
        Err(_) => return ptr::null_mut(),
    };
    let key = Key::<Aes256Gcm>::from_slice(&key_bytes);

    let cipher = Aes256Gcm::new(key);
    let nonce = Aes256Gcm::generate_nonce(&mut OsRng);

    let ciphertext = match cipher.encrypt(&nonce, text.as_bytes()) {
        Ok(c) => c,
        Err(_) => return ptr::null_mut(),
    };

    // 将 nonce + 密文合并进行 base64 编码
    let mut combined = Vec::with_capacity(nonce.len() + ciphertext.len());
    combined.extend_from_slice(&nonce);
    combined.extend_from_slice(&ciphertext);

    let result = BASE64.encode(&combined);
    CString::new(result).unwrap_or_default().into_raw()
}

/// 使用 AES-256-GCM 解密消息
/// 返回明文字符串，需要调用 rust_free_string 释放
#[no_mangle]
pub extern "C" fn rust_decrypt_message(
    ciphertext_b64: *const c_char,
    key_base64: *const c_char,
) -> *mut c_char {
    if ciphertext_b64.is_null() || key_base64.is_null() {
        return ptr::null_mut();
    }

    let ct_str = match unsafe { CStr::from_ptr(ciphertext_b64) }.to_str() {
        Ok(s) => s,
        Err(_) => return ptr::null_mut(),
    };
    let key_str = match unsafe { CStr::from_ptr(key_base64) }.to_str() {
        Ok(s) => s,
        Err(_) => return ptr::null_mut(),
    };

    // 解码密钥
    let key_bytes = match BASE64.decode(key_str) {
        Ok(b) => b,
        Err(_) => return ptr::null_mut(),
    };
    let key = Key::<Aes256Gcm>::from_slice(&key_bytes);

    // 解码密文
    let combined = match BASE64.decode(ct_str) {
        Ok(c) => c,
        Err(_) => return ptr::null_mut(),
    };
    if combined.len() < 12 {
        return ptr::null_mut();
    }

    let (nonce_bytes, ciphertext) = combined.split_at(12);
    let nonce = Nonce::from_slice(nonce_bytes);

    let cipher = Aes256Gcm::new(key);
    let plaintext = match cipher.decrypt(nonce, ciphertext) {
        Ok(p) => p,
        Err(_) => return ptr::null_mut(),
    };

    let result = String::from_utf8(plaintext).unwrap_or_default();
    CString::new(result).unwrap_or_default().into_raw()
}

/// 生成一个 AES-256 密钥（base64 编码）
#[no_mangle]
pub extern "C" fn rust_generate_key() -> *mut c_char {
    let key = Aes256Gcm::generate_key(&mut OsRng);
    let encoded = BASE64.encode(key.as_slice());
    CString::new(encoded).unwrap_or_default().into_raw()
}
