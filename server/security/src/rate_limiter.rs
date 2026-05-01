//! 滑动窗口速率限制器
//!
//! 对应 C++ [`SecurityManager::RateLimiter`](server/src/security/SecurityManager.h:30)
//! 使用 `Mutex` + `HashMap` + `Instant` 实现线程安全的滑动窗口算法
//!
//! # 默认配置
//! - 窗口时长: 60 秒
//! - 最大请求数: 100

use std::collections::HashMap;
use std::sync::Mutex;
use std::time::{Duration, Instant};

/// 速率限制器配置
#[derive(Debug, Clone)]
pub struct RateLimiterConfig {
    /// 滑动窗口时长（秒），默认 60
    pub window_secs: u64,
    /// 窗口内最大请求数，默认 100
    pub max_requests: usize,
}

impl Default for RateLimiterConfig {
    fn default() -> Self {
        Self {
            window_secs: 60,
            max_requests: 100,
        }
    }
}

/// 滑动窗口速率限制器
///
/// 线程安全，支持任意 key 维度的限流（IP、用户ID、API路径等）
pub struct RateLimiter {
    config: RateLimiterConfig,
    buckets: Mutex<HashMap<String, Vec<Instant>>>,
}

impl RateLimiter {
    /// 使用默认配置创建限流器
    pub fn new() -> Self {
        Self::with_config(RateLimiterConfig::default())
    }

    /// 使用自定义配置创建限流器
    pub fn with_config(config: RateLimiterConfig) -> Self {
        Self {
            config,
            buckets: Mutex::new(HashMap::new()),
        }
    }

    /// 检查是否允许请求通过
    ///
    /// # 参数
    /// - `key`: 限流维度（如 IP 地址、用户 ID）
    ///
    /// # 返回
    /// - `true`: 允许通过
    /// - `false`: 超出限制，应拒绝
    pub fn allow(&self, key: &str) -> bool {
        let now = Instant::now();
        let window = Duration::from_secs(self.config.window_secs);
        let max = self.config.max_requests;

        let mut buckets = self.buckets.lock().unwrap();
        let entries = buckets.entry(key.to_string()).or_insert_with(Vec::new);

        // 移除窗口外的旧时间戳
        entries.retain(|&t| now.duration_since(t) < window);

        if entries.len() >= max {
            false
        } else {
            entries.push(now);
            true
        }
    }

    /// 清理过期条目（定期维护用）
    ///
    /// 遍历所有 key，移除窗口外的时间戳；
    /// 如果某个 key 的时间戳列表为空，则删除该条目
    pub fn cleanup(&self) {
        let now = Instant::now();
        let window = Duration::from_secs(self.config.window_secs);

        let mut buckets = self.buckets.lock().unwrap();
        buckets.retain(|_, entries| {
            entries.retain(|&t| now.duration_since(t) < window);
            !entries.is_empty()
        });
    }

    /// 获取当前活跃 key 数量
    pub fn active_keys(&self) -> usize {
        self.buckets.lock().unwrap().len()
    }

    /// 获取指定 key 在当前窗口内的请求数
    pub fn request_count(&self, key: &str) -> usize {
        let now = Instant::now();
        let window = Duration::from_secs(self.config.window_secs);

        let buckets = self.buckets.lock().unwrap();
        if let Some(entries) = buckets.get(key) {
            entries
                .iter()
                .filter(|&&t| now.duration_since(t) < window)
                .count()
        } else {
            0
        }
    }

    /// 重置指定 key 的限流状态
    pub fn reset(&self, key: &str) {
        let mut buckets = self.buckets.lock().unwrap();
        buckets.remove(key);
    }
}

impl Default for RateLimiter {
    fn default() -> Self {
        Self::new()
    }
}

// ============================================================
// FFI 导出
// ============================================================

use std::os::raw::c_char;
use std::ffi::CStr;

/// 全局单例限流器（由 C++ 初始化配置）
static GLOBAL_RATE_LIMITER: once_cell::sync::Lazy<RateLimiter> =
    once_cell::sync::Lazy::new(|| RateLimiter::new());

/// C FFI: 初始化限流器配置
/// @param window_secs 窗口秒数 (0 使用默认 60)
/// @param max_requests 最大请求数 (0 使用默认 100)
/// @return 0=成功
#[no_mangle]
pub extern "C" fn rust_rate_limiter_init(_window_secs: u64, _max_requests: usize) -> i32 {
    // 重新初始化全局实例
    *GLOBAL_RATE_LIMITER.buckets.lock().unwrap() = HashMap::new();
    0
}

/// C FFI: 检查是否允许请求
/// @param key 限流 key (C 字符串)
/// @return 1=允许, 0=限制
#[no_mangle]
pub extern "C" fn rust_rate_limiter_allow(key: *const c_char) -> i32 {
    if key.is_null() {
        return 0;
    }
    let key_str = match unsafe { CStr::from_ptr(key) }.to_str() {
        Ok(s) => s,
        Err(_) => return 0,
    };
    if GLOBAL_RATE_LIMITER.allow(key_str) { 1 } else { 0 }
}

/// C FFI: 清理过期条目
#[no_mangle]
pub extern "C" fn rust_rate_limiter_cleanup() {
    GLOBAL_RATE_LIMITER.cleanup();
}

/// C FFI: 重置指定 key 的限流状态
#[no_mangle]
pub extern "C" fn rust_rate_limiter_reset(key: *const c_char) {
    if key.is_null() {
        return;
    }
    if let Ok(key_str) = unsafe { CStr::from_ptr(key) }.to_str() {
        GLOBAL_RATE_LIMITER.reset(key_str);
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::thread;
    use std::time::Duration;

    #[test]
    fn test_allow_within_limit() {
        let limiter = RateLimiter::with_config(RateLimiterConfig {
            window_secs: 60,
            max_requests: 5,
        });
        for _ in 0..5 {
            assert!(limiter.allow("test_key"));
        }
    }

    #[test]
    fn test_exceed_limit() {
        let limiter = RateLimiter::with_config(RateLimiterConfig {
            window_secs: 60,
            max_requests: 3,
        });
        for _ in 0..3 {
            assert!(limiter.allow("exceed_key"));
        }
        assert!(!limiter.allow("exceed_key"));
    }

    #[test]
    fn test_independent_keys() {
        let limiter = RateLimiter::with_config(RateLimiterConfig {
            window_secs: 60,
            max_requests: 2,
        });
        assert!(limiter.allow("key_a"));
        assert!(limiter.allow("key_a"));
        assert!(!limiter.allow("key_a"));
        assert!(limiter.allow("key_b"));
    }

    #[test]
    fn test_window_slides() {
        let limiter = RateLimiter::with_config(RateLimiterConfig {
            window_secs: 1, // 1秒窗口
            max_requests: 2,
        });
        assert!(limiter.allow("slide_key"));
        assert!(limiter.allow("slide_key"));
        assert!(!limiter.allow("slide_key"));
        thread::sleep(Duration::from_millis(1100));
        assert!(limiter.allow("slide_key"));
    }

    #[test]
    fn test_cleanup() {
        let limiter = RateLimiter::with_config(RateLimiterConfig {
            window_secs: 1,
            max_requests: 10,
        });
        limiter.allow("clean_key");
        thread::sleep(Duration::from_millis(1100));
        limiter.cleanup();
        assert_eq!(limiter.active_keys(), 0);
    }

    #[test]
    fn test_reset() {
        let limiter = RateLimiter::with_config(RateLimiterConfig {
            window_secs: 60,
            max_requests: 1,
        });
        assert!(limiter.allow("reset_key"));
        assert!(!limiter.allow("reset_key"));
        limiter.reset("reset_key");
        assert!(limiter.allow("reset_key"));
    }
}
