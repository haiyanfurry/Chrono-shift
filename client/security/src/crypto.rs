//! 客户端加密模块
//! 端到端加密（E2E）密钥对生成和加解密
//! ASM 私有混淆加密（512 位密钥对称流密码）

use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use std::ptr;

use aes_gcm::{Aes256Gcm, Key, Nonce};
use aes_gcm::aead::{Aead, AeadCore, KeyInit, OsRng};
use base64::{Engine as _, engine::general_purpose::STANDARD as BASE64};

use crate::sanitizer;
use crate::asm_bridge;

/// 生成 E2E 密钥对（返回公钥 base64）
/// 需要调用 rust_client_free_string 释放
#[no_mangle]
pub extern "C" fn rust_client_generate_keypair() -> *mut c_char {
    let key = Aes256Gcm::generate_key(&mut OsRng);
    let encoded = BASE64.encode(key.as_slice());
    CString::new(encoded).unwrap_or_default().into_raw()
}

/// 使用对方的公钥加密消息（E2E）
///
/// 安全校验:
/// - 明文长度在 MESSAGE_MAX_LEN 内
/// - 公钥是合法 Base64
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

    // ── 安全校验：消息长度 ──
    if !sanitizer::validate_message_length(text) {
        return ptr::null_mut(); // 消息过长或为空
    }

    // ── 安全校验：公钥 Base64 格式 ──
    if !sanitizer::validate_token(key_str) {
        return ptr::null_mut(); // 公钥包含非法字符
    }

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
///
/// 安全校验:
/// - 密文 Base64 格式校验
/// - 私钥 Base64 格式校验
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

    // ── 安全校验：Base64 格式 ──
    if !sanitizer::validate_token(ct_str) || !sanitizer::validate_token(key_str) {
        return ptr::null_mut();
    }

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

// ═══════════════════════════════════════════════════════════════
// ASM 私有混淆加密（512 位密钥对称流密码）
// ═══════════════════════════════════════════════════════════════

/// 内部：将 128 字符 hex 字符串解析为 64 字节密钥
fn parse_512bit_key_hex(hex_str: &str) -> Result<[u8; 64], String> {
    if hex_str.len() != 128 {
        return Err(format!("密钥 hex 长度应为 128，实际为 {}", hex_str.len()));
    }
    let mut key = [0u8; 64];
    for i in 0..64 {
        let byte_str = &hex_str[i * 2..i * 2 + 2];
        key[i] = u8::from_str_radix(byte_str, 16)
            .map_err(|_| format!("第 {} 字节 hex 解析失败: {}", i, byte_str))?;
    }
    Ok(key)
}

/// 使用 ASM 混淆加密消息
///
/// 参数:
/// - `plaintext_b64`: 明文的 Base64 编码
/// - `key_hex`: 512 位密钥的十六进制字符串（128 个 hex 字符）
///
/// 返回: Base64 编码的密文（通过 rust_client_free_string 释放）
#[no_mangle]
pub extern "C" fn rust_client_obfuscate_message(
    plaintext_b64: *const c_char,
    key_hex: *const c_char,
) -> *mut c_char {
    if plaintext_b64.is_null() || key_hex.is_null() {
        return ptr::null_mut();
    }

    let b64_str = match unsafe { CStr::from_ptr(plaintext_b64) }.to_str() {
        Ok(s) => s,
        Err(_) => return ptr::null_mut(),
    };
    let key_str = match unsafe { CStr::from_ptr(key_hex) }.to_str() {
        Ok(s) => s,
        Err(_) => return ptr::null_mut(),
    };

    // 安全校验：Base64 格式
    if !sanitizer::validate_token(b64_str) || !sanitizer::validate_token(key_str) {
        return ptr::null_mut();
    }

    // 解析密钥（512 位 hex）
    let key = match parse_512bit_key_hex(key_str) {
        Ok(k) => k,
        Err(_) => return ptr::null_mut(),
    };

    // Base64 解码明文
    let plaintext = match BASE64.decode(b64_str) {
        Ok(b) => b,
        Err(_) => return ptr::null_mut(),
    };

    // 调用 ASM 混淆
    let ciphertext = match asm_bridge::obfuscate(&plaintext, &key) {
        Ok(c) => c,
        Err(_) => return ptr::null_mut(),
    };

    CString::new(BASE64.encode(&ciphertext)).unwrap_or_default().into_raw()
}

/// 使用 ASM 混淆解密消息
///
/// 参数:
/// - `ciphertext_b64`: 密文的 Base64 编码
/// - `key_hex`: 512 位密钥的十六进制字符串（128 个 hex 字符）
///
/// 返回: Base64 编码的明文（通过 rust_client_free_string 释放）
#[no_mangle]
pub extern "C" fn rust_client_deobfuscate_message(
    ciphertext_b64: *const c_char,
    key_hex: *const c_char,
) -> *mut c_char {
    if ciphertext_b64.is_null() || key_hex.is_null() {
        return ptr::null_mut();
    }

    let ct_str = match unsafe { CStr::from_ptr(ciphertext_b64) }.to_str() {
        Ok(s) => s,
        Err(_) => return ptr::null_mut(),
    };
    let key_str = match unsafe { CStr::from_ptr(key_hex) }.to_str() {
        Ok(s) => s,
        Err(_) => return ptr::null_mut(),
    };

    // 安全校验：Base64 格式
    if !sanitizer::validate_token(ct_str) || !sanitizer::validate_token(key_str) {
        return ptr::null_mut();
    }

    // 解析密钥（512 位 hex）
    let key = match parse_512bit_key_hex(key_str) {
        Ok(k) => k,
        Err(_) => return ptr::null_mut(),
    };

    // Base64 解码密文
    let ciphertext = match BASE64.decode(ct_str) {
        Ok(b) => b,
        Err(_) => return ptr::null_mut(),
    };

    // 调用 ASM 反混淆
    let plaintext = match asm_bridge::deobfuscate(&ciphertext, &key) {
        Ok(p) => p,
        Err(_) => return ptr::null_mut(),
    };

    CString::new(BASE64.encode(&plaintext)).unwrap_or_default().into_raw()
}
