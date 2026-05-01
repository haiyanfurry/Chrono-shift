//! 会话管理器
//!
//! 对应 C++ [`SecurityManager::SessionManager`](server/src/security/SecurityManager.h:106)
//! 线程安全，支持会话创建、验证、过期清理
//!
//! # 安全设计
//! - 会话 Token: 32 字节随机数 → 64 字符 hex 字符串
//! - 过期时间: 可配置（默认 1 小时）
//! - 使用 `RwLock` 实现读写分离：读多写少场景优化

use rand::Rng;
use std::collections::HashMap;
use std::sync::RwLock;
use std::time::{Duration, SystemTime, UNIX_EPOCH};

/// 会话条目
#[derive(Debug, Clone)]
struct SessionEntry {
    /// 用户 ID
    user_id: String,
    /// 创建时间戳（Unix 秒）
    created_at: u64,
    /// 过期时间戳（Unix 秒）
    expires_at: u64,
    /// 额外数据（JSON 格式，可扩展）
    metadata: String,
}

/// 会话管理器配置
#[derive(Debug, Clone)]
pub struct SessionConfig {
    /// 会话超时时间（秒），默认 3600（1 小时）
    pub timeout_secs: u64,
}

impl Default for SessionConfig {
    fn default() -> Self {
        Self { timeout_secs: 3600 }
    }
}

/// 会话管理器
///
/// 线程安全：读操作使用 `RwLock` 的读锁，写操作使用写锁
pub struct SessionManager {
    config: SessionConfig,
    sessions: RwLock<HashMap<String, SessionEntry>>,
}

impl SessionManager {
    /// 使用默认配置创建
    pub fn new() -> Self {
        Self::with_config(SessionConfig::default())
    }

    /// 使用自定义配置创建
    pub fn with_config(config: SessionConfig) -> Self {
        Self {
            config,
            sessions: RwLock::new(HashMap::new()),
        }
    }

    /// 当前时间戳（Unix 秒）
    fn now_secs() -> u64 {
        SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .unwrap_or(Duration::from_secs(0))
            .as_secs()
    }

    /// 生成随机会话 Token
    ///
    /// 32 字节随机数 → 64 字符 hex 字符串
    fn generate_token() -> String {
        let mut rng = rand::thread_rng();
        let bytes: [u8; 32] = rng.gen();
        bytes.iter().map(|b| format!("{:02x}", b)).collect()
    }

    /// 创建新会话
    ///
    /// # 参数
    /// - `user_id`: 用户 ID
    /// - `metadata`: 可选元数据（JSON 格式）
    ///
    /// # 返回
    /// 会话 Token 字符串（64 字符 hex）
    pub fn create_session(&self, user_id: &str, metadata: &str) -> String {
        let token = Self::generate_token();
        let now = Self::now_secs();
        let entry = SessionEntry {
            user_id: user_id.to_string(),
            created_at: now,
            expires_at: now + self.config.timeout_secs,
            metadata: metadata.to_string(),
        };

        let mut sessions = self.sessions.write().unwrap();
        sessions.insert(token.clone(), entry);
        token
    }

    /// 验证会话
    ///
    /// # 参数
    /// - `token`: 会话 Token
    ///
    /// # 返回
    /// - `Some(user_id)`: 会话有效
    /// - `None`: 会话无效或已过期
    pub fn validate_session(&self, token: &str) -> Option<String> {
        let sessions = self.sessions.read().unwrap();
        if let Some(entry) = sessions.get(token) {
            if Self::now_secs() < entry.expires_at {
                return Some(entry.user_id.clone());
            }
        }
        None
    }

    /// 移除会话
    pub fn remove_session(&self, token: &str) {
        let mut sessions = self.sessions.write().unwrap();
        sessions.remove(token);
    }

    /// 清理过期会话
    ///
    /// 移除所有已过期的会话条目
    pub fn cleanup_expired(&self) {
        let now = Self::now_secs();
        let mut sessions = self.sessions.write().unwrap();
        sessions.retain(|_, entry| now < entry.expires_at);
    }

    /// 获取当前活跃会话数
    pub fn active_sessions(&self) -> usize {
        let now = Self::now_secs();
        let sessions = self.sessions.read().unwrap();
        sessions.iter().filter(|(_, e)| now < e.expires_at).count()
    }

    /// 获取指定用户的所有会话 token
    pub fn get_user_sessions(&self, user_id: &str) -> Vec<String> {
        let now = Self::now_secs();
        let sessions = self.sessions.read().unwrap();
        sessions
            .iter()
            .filter(|(_, e)| e.user_id == user_id && now < e.expires_at)
            .map(|(t, _)| t.clone())
            .collect()
    }

    /// 移除指定用户的所有会话（强制下线）
    pub fn remove_user_sessions(&self, user_id: &str) {
        let mut sessions = self.sessions.write().unwrap();
        sessions.retain(|_, e| e.user_id != user_id);
    }

    /// 刷新会话过期时间（续期）
    pub fn refresh_session(&self, token: &str) -> bool {
        let now = Self::now_secs();
        let mut sessions = self.sessions.write().unwrap();
        if let Some(entry) = sessions.get_mut(token) {
            if now < entry.expires_at {
                entry.expires_at = now + self.config.timeout_secs;
                return true;
            }
        }
        false
    }

    /// 获取会话的创建时间
    pub fn get_session_created_at(&self, token: &str) -> Option<u64> {
        let sessions = self.sessions.read().unwrap();
        sessions.get(token).map(|e| e.created_at)
    }
}

impl Default for SessionManager {
    fn default() -> Self {
        Self::new()
    }
}

// ============================================================
// FFI 导出
// ============================================================

use std::ffi::{CStr, CString};
use std::os::raw::c_char;

/// 全局单例会话管理器
static GLOBAL_SESSION_MANAGER: once_cell::sync::Lazy<SessionManager> =
    once_cell::sync::Lazy::new(|| SessionManager::new());

/// C FFI: 初始化会话管理器
/// @param timeout_secs 超时秒数（0 使用默认 3600）
/// @return 0=成功
#[no_mangle]
pub extern "C" fn rust_session_init(_timeout_secs: u64) -> i32 {
    // 清空旧会话
    *GLOBAL_SESSION_MANAGER.sessions.write().unwrap() = HashMap::new();
    0
}

/// C FFI: 创建会话
/// @param user_id 用户 ID
/// @param metadata 元数据（可为空）
/// @return 分配的 token 字符串，需调用 rust_free_string 释放
#[no_mangle]
pub extern "C" fn rust_session_create(user_id: *const c_char, metadata: *const c_char) -> *mut c_char {
    if user_id.is_null() {
        return std::ptr::null_mut();
    }
    let uid = match unsafe { CStr::from_ptr(user_id) }.to_str() {
        Ok(s) => s,
        Err(_) => return std::ptr::null_mut(),
    };
    let meta = if metadata.is_null() {
        ""
    } else {
        unsafe { CStr::from_ptr(metadata) }.to_str().unwrap_or("")
    };
    let token = GLOBAL_SESSION_MANAGER.create_session(uid, meta);
    let c_str = CString::new(token).unwrap();
    c_str.into_raw()
}

/// C FFI: 验证会话
/// @param token 会话 token
/// @return 分配的 user_id 字符串，需调用 rust_free_string 释放；无效返回 null
#[no_mangle]
pub extern "C" fn rust_session_validate(token: *const c_char) -> *mut c_char {
    if token.is_null() {
        return std::ptr::null_mut();
    }
    let tok = match unsafe { CStr::from_ptr(token) }.to_str() {
        Ok(s) => s,
        Err(_) => return std::ptr::null_mut(),
    };
    match GLOBAL_SESSION_MANAGER.validate_session(tok) {
        Some(uid) => {
            let c_str = CString::new(uid).unwrap();
            c_str.into_raw()
        }
        None => std::ptr::null_mut(),
    }
}

/// C FFI: 移除会话
#[no_mangle]
pub extern "C" fn rust_session_remove(token: *const c_char) {
    if token.is_null() {
        return;
    }
    if let Ok(tok) = unsafe { CStr::from_ptr(token) }.to_str() {
        GLOBAL_SESSION_MANAGER.remove_session(tok);
    }
}

/// C FFI: 清理过期会话
#[no_mangle]
pub extern "C" fn rust_session_cleanup() {
    GLOBAL_SESSION_MANAGER.cleanup_expired();
}

/// C FFI: 刷新会话过期时间
/// @return 1=成功, 0=失败或已过期
#[no_mangle]
pub extern "C" fn rust_session_refresh(token: *const c_char) -> i32 {
    if token.is_null() {
        return 0;
    }
    let tok = match unsafe { CStr::from_ptr(token) }.to_str() {
        Ok(s) => s,
        Err(_) => return 0,
    };
    if GLOBAL_SESSION_MANAGER.refresh_session(tok) { 1 } else { 0 }
}

/// C FFI: 获取活跃会话数
#[no_mangle]
pub extern "C" fn rust_session_active_count() -> i32 {
    GLOBAL_SESSION_MANAGER.active_sessions() as i32
}

/// C FFI: 移除指定用户的所有会话
#[no_mangle]
pub extern "C" fn rust_session_remove_user(user_id: *const c_char) {
    if user_id.is_null() {
        return;
    }
    if let Ok(uid) = unsafe { CStr::from_ptr(user_id) }.to_str() {
        GLOBAL_SESSION_MANAGER.remove_user_sessions(uid);
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::thread;
    use std::time::Duration;

    #[test]
    fn test_create_and_validate_session() {
        let mgr = SessionManager::new();
        let token = mgr.create_session("user_123", "{}");
        assert!(!token.is_empty());
        assert_eq!(token.len(), 64);
        assert_eq!(mgr.validate_session(&token), Some("user_123".into()));
    }

    #[test]
    fn test_remove_session() {
        let mgr = SessionManager::new();
        let token = mgr.create_session("user_456", "");
        mgr.remove_session(&token);
        assert_eq!(mgr.validate_session(&token), None);
    }

    #[test]
    fn test_expired_session() {
        let mgr = SessionManager::with_config(SessionConfig { timeout_secs: 1 });
        let token = mgr.create_session("user_789", "");
        assert!(mgr.validate_session(&token).is_some());
        thread::sleep(Duration::from_millis(1100));
        assert!(mgr.validate_session(&token).is_none());
    }

    #[test]
    fn test_cleanup_expired() {
        let mgr = SessionManager::with_config(SessionConfig { timeout_secs: 1 });
        let _token = mgr.create_session("user_abc", "");
        thread::sleep(Duration::from_millis(1100));
        assert_eq!(mgr.active_sessions(), 0);
        mgr.cleanup_expired();
        assert_eq!(mgr.active_sessions(), 0);
    }

    #[test]
    fn test_refresh_session() {
        let mgr = SessionManager::with_config(SessionConfig { timeout_secs: 2 });
        let token = mgr.create_session("user_refresh", "");
        thread::sleep(Duration::from_millis(1100));
        assert!(mgr.refresh_session(&token));
        thread::sleep(Duration::from_millis(1100));
        // 如果没 refresh，此时已过期；refresh 后应该仍然有效
        assert!(mgr.validate_session(&token).is_some());
    }

    #[test]
    fn test_remove_user_sessions() {
        let mgr = SessionManager::new();
        let t1 = mgr.create_session("user_xyz", "");
        let t2 = mgr.create_session("user_xyz", "");
        mgr.remove_user_sessions("user_xyz");
        assert!(mgr.validate_session(&t1).is_none());
        assert!(mgr.validate_session(&t2).is_none());
    }

    #[test]
    fn test_get_user_sessions() {
        let mgr = SessionManager::new();
        let t1 = mgr.create_session("user_sess", "");
        let t2 = mgr.create_session("user_sess", "");
        let sessions = mgr.get_user_sessions("user_sess");
        assert_eq!(sessions.len(), 2);
        assert!(sessions.contains(&t1));
        assert!(sessions.contains(&t2));
    }
}
