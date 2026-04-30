//! 会话管理模块
//! 管理登录令牌、会话状态

use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use std::ptr;
use std::sync::Mutex;

use once_cell::sync::Lazy;
use rand::RngCore;

/// 会话状态
struct Session {
    user_id: String,
    username: String,
    token: String,
    is_logged_in: bool,
}

static SESSION: Lazy<Mutex<Session>> = Lazy::new(|| {
    Mutex::new(Session {
        user_id: String::new(),
        username: String::new(),
        token: String::new(),
        is_logged_in: false,
    })
});

/// 保存会话令牌
#[no_mangle]
pub extern "C" fn rust_session_save(
    user_id: *const c_char,
    username: *const c_char,
    token: *const c_char,
) -> i32 {
    if user_id.is_null() || username.is_null() || token.is_null() {
        return -1;
    }

    let uid = unsafe { CStr::from_ptr(user_id) }.to_str().unwrap_or("");
    let name = unsafe { CStr::from_ptr(username) }.to_str().unwrap_or("");
    let t = unsafe { CStr::from_ptr(token) }.to_str().unwrap_or("");

    let mut session = SESSION.lock().unwrap();
    session.user_id = uid.to_string();
    session.username = name.to_string();
    session.token = t.to_string();
    session.is_logged_in = true;

    0
}

/// 获取当前会话令牌
#[no_mangle]
pub extern "C" fn rust_session_get_token() -> *mut c_char {
    let session = SESSION.lock().unwrap();
    if session.is_logged_in {
        CString::new(session.token.clone()).unwrap_or_default().into_raw()
    } else {
        ptr::null_mut()
    }
}

/// 检查是否已登录
#[no_mangle]
pub extern "C" fn rust_session_is_logged_in() -> i32 {
    let session = SESSION.lock().unwrap();
    if session.is_logged_in { 1 } else { 0 }
}

/// 清除会话
#[no_mangle]
pub extern "C" fn rust_session_clear() {
    let mut session = SESSION.lock().unwrap();
    session.user_id.clear();
    session.username.clear();
    session.token.clear();
    session.is_logged_in = false;
}
