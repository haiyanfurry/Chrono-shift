//! 密钥管理模块
//! 处理服务器密钥的生成、存储和加载

use std::fs;
use std::path::Path;
use std::sync::OnceLock;

use aes_gcm::Aes256Gcm;
use aes_gcm::aead::{KeyInit, OsRng};
use base64::{Engine as _, engine::general_purpose::STANDARD as BASE64};
use rand::RngCore;

/// 服务器主密钥
struct KeyStore {
    server_key: [u8; 32],
}

static KEY_STORE: OnceLock<KeyStore> = OnceLock::new();

/// 初始化密钥存储
pub fn init_key_store(config_path: &str) -> Result<(), String> {
    let key_path = Path::new(config_path).join("server_key.bin");

    let server_key = if key_path.exists() {
        let bytes = fs::read(&key_path).map_err(|e| format!("Failed to read key: {}", e))?;
        if bytes.len() != 32 {
            return Err("Invalid key file size".to_string());
        }
        let mut key = [0u8; 32];
        key.copy_from_slice(&bytes);
        key
    } else {
        let mut key = [0u8; 32];
        OsRng.fill_bytes(&mut key);
        fs::write(&key_path, &key).map_err(|e| format!("Failed to write key: {}", e))?;
        key
    };

    let _ = KEY_STORE.set(KeyStore { server_key });
    Ok(())
}

/// 获取服务器公钥（base64 编码）
pub fn get_server_public_key() -> Option<String> {
    let store = KEY_STORE.get()?;
    Some(BASE64.encode(&store.server_key))
}

/// 获取 JWT 签名密钥（原始 32 字节）
pub fn get_jwt_secret() -> Option<[u8; 32]> {
    let store = KEY_STORE.get()?;
    Some(store.server_key)
}
