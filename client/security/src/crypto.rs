//! 客户端加密模块
//! 端到端加密（E2E）密钥对生成和加解密

use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use std::ptr;

use aes_gcm::{Aes256Gcm, Key, Nonce};
use aes_gcm::aead::{Aead, KeyInit, OsRng};
use base64::{Engine as _, engine::general_purpose::STANDARD as BASE64};

/// 生成 E2E 密钥对（返回公钥 base64）
/// 需要调用 rust_client_free_string 释放
#[no_mangle]
pub extern "C" fn rust_client_generate_keypair() -> *mut c_char {
    let key = Aes256Gcm::generate_key(&mut OsRng);
    let encoded = BASE64.encode(key.as_slice());
    CString::new(encoded).unwrap_or_default().into_raw()
}

/// 使用对方的公钥加密消息（E2E）
#[no_mangle]
pub extern "C" fn rust_client_encrypt_e2e(
    plaintext: *const c_char,
    pubkey_b64: *const c_char,
) -> *mut c_char {
    if plaintext.is_null() || pubkey_b64.is_null() {
        return ptr::null_mut();
    }

    let text = match unsafe { CStr::from_ptr(plaintext) }.to_str() {
        Ok(s) => s,
        Err(_) => return ptr::null_mut(),
    };
    let key_str = match unsafe { CStr::from_ptr(pubkey_b64) }.to_str() {
        Ok(s) => s,
        Err(_) => return ptr::null_mut(),
    };

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

    let mut combined = Vec::with_capacity(nonce.len() + ciphertext.len());
    combined.extend_from_slice(&nonce);
    combined.extend_from_slice(&ciphertext);

    CString::new(BASE64.encode(&combined)).unwrap_or_default().into_raw()
}

/// 使用自己的私钥解密消息（E2E）
#[no_mangle]
pub extern "C" fn rust_client_decrypt_e2e(
    ciphertext_b64: *const c_char,
    privkey_b64: *const c_char,
) -> *mut c_char {
    if ciphertext_b64.is_null() || privkey_b64.is_null() {
        return ptr::null_mut();
    }

    let ct_str = match unsafe { CStr::from_ptr(ciphertext_b64) }.to_str() {
        Ok(s) => s,
        Err(_) => return ptr::null_mut(),
    };
    let key_str = match unsafe { CStr::from_ptr(privkey_b64) }.to_str() {
        Ok(s) => s,
        Err(_) => return ptr::null_mut(),
    };

    let key_bytes = match BASE64.decode(key_str) {
        Ok(b) => b,
        Err(_) => return ptr::null_mut(),
    };
    let key = Key::<Aes256Gcm>::from_slice(&key_bytes);

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

    CString::new(String::from_utf8(plaintext).unwrap_or_default())
        .unwrap_or_default()
        .into_raw()
}
