//! Chrono-shift 服务端安全模块
//! 通过 extern "C" FFI 导出函数供 C99 后端调用

pub mod crypto;
pub mod auth;
pub mod password;
pub mod key_mgmt;

use std::ffi::{CStr, CString};
use std::os::raw::c_char;

/// 初始化安全模块
/// 返回 0 表示成功，-1 表示失败
#[no_mangle]
pub extern "C" fn rust_server_init(config_path: *const c_char) -> i32 {
    if config_path.is_null() {
        return -1;
    }
    let path = match unsafe { CStr::from_ptr(config_path) }.to_str() {
        Ok(s) => s.to_string(),
        Err(_) => return -1,
    };
    // 初始化日志和密钥管理
    match key_mgmt::init_key_store(&path) {
        Ok(_) => 0,
        Err(_) => -1,
    }
}

/// 释放 Rust 分配的字符串内存
/// C 端调用此函数释放由 Rust 返回的字符串
#[no_mangle]
pub extern "C" fn rust_free_string(s: *mut c_char) {
    if !s.is_null() {
        unsafe {
            let _ = CString::from_raw(s);
        }
    }
}

/// 获取版本信息
#[no_mangle]
pub extern "C" fn rust_version() -> *mut c_char {
    let version = CString::new("0.1.0").unwrap();
    version.into_raw()
}
