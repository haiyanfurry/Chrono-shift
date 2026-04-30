//! 客户端安全存储
//! 使用系统级加密保护本地存储的敏感数据（令牌、密钥等）

use std::fs;
use std::path::PathBuf;
use std::sync::OnceLock;

use aes_gcm::{Aes256Gcm, Key, Nonce};
use aes_gcm::aead::{Aead, KeyInit, OsRng};

static STORAGE_DIR: OnceLock<PathBuf> = OnceLock::new();

/// 密钥文件路径
fn get_key_path() -> PathBuf {
    let dir = STORAGE_DIR.get().expect("Secure storage not initialized");
    dir.join(".chrono_master_key")
}

/// 数据文件路径
fn get_data_path() -> PathBuf {
    let dir = STORAGE_DIR.get().expect("Secure storage not initialized");
    dir.join(".chrono_secure_data")
}

/// 初始化安全存储
pub fn init_secure_storage(app_data_path: &str) -> Result<(), String> {
    let path = PathBuf::from(app_data_path).join("secure");
    fs::create_dir_all(&path).map_err(|e| format!("Failed to create secure dir: {}", e))?;
    
    STORAGE_DIR.set(path).map_err(|_| "Storage already initialized".to_string())?;

    // 如果密钥不存在则生成
    if !get_key_path().exists() {
        let key = Aes256Gcm::generate_key(&mut OsRng);
        fs::write(get_key_path(), key.as_slice())
            .map_err(|e| format!("Failed to write key: {}", e))?;
    }

    Ok(())
}

/// 获取或创建设备密钥
fn get_device_key() -> Result<[u8; 32], String> {
    let key_bytes = fs::read(get_key_path())
        .map_err(|e| format!("Failed to read key: {}", e))?;
    if key_bytes.len() != 32 {
        return Err("Invalid key length".to_string());
    }
    let mut key = [0u8; 32];
    key.copy_from_slice(&key_bytes);
    Ok(key)
}

/// 安全存储数据
fn secure_store(key: &str, value: &str) -> Result<(), String> {
    let device_key = get_device_key()?;
    let key = Key::<Aes256Gcm>::from_slice(&device_key);
    let cipher = Aes256Gcm::new(key);
    let nonce = Aes256Gcm::generate_nonce(&mut OsRng);

    let entry = format!("{}={}", key, value);
    let ciphertext = cipher.encrypt(&nonce, entry.as_bytes())
        .map_err(|e| format!("Encryption failed: {}", e))?;

    let mut combined = Vec::with_capacity(nonce.len() + ciphertext.len());
    combined.extend_from_slice(&nonce);
    combined.extend_from_slice(&ciphertext);

    fs::write(get_data_path(), &combined)
        .map_err(|e| format!("Failed to write secure data: {}", e))?;

    Ok(())
}

/// 安全读取数据
fn secure_load(target_key: &str) -> Result<Option<String>, String> {
    if !get_data_path().exists() {
        return Ok(None);
    }

    let device_key = get_device_key()?;
    let key = Key::<Aes256Gcm>::from_slice(&device_key);
    let cipher = Aes256Gcm::new(key);

    let combined = fs::read(get_data_path())
        .map_err(|e| format!("Failed to read secure data: {}", e))?;
    if combined.len() < 12 {
        return Ok(None);
    }

    let (nonce_bytes, ciphertext) = combined.split_at(12);
    let nonce = Nonce::from_slice(nonce_bytes);

    let plaintext = cipher.decrypt(nonce, ciphertext)
        .map_err(|e| format!("Decryption failed: {}", e))?;

    let entry = String::from_utf8(plaintext)
        .map_err(|_| "Invalid UTF-8".to_string())?;

    if let Some(pos) = entry.find('=') {
        let k = &entry[..pos];
        if k == target_key {
            return Ok(Some(entry[pos + 1..].to_string()));
        }
    }

    Ok(None)
}
