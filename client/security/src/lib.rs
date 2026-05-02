//! Chrono-shift 客户端安全模块
//! 通过 extern "C" FFI 导出函数供 C99 客户端宿主调用

pub mod asm_bridge;
pub mod crypto;
pub mod sanitizer;
pub mod secure_storage;
pub mod session;

use std::ffi::CString;
use std::os::raw::c_char;

/// 初始化客户端安全模块
#[no_mangle]
pub extern "C" fn rust_client_init(app_data_path: *const c_char) -> i32 {
    use std::ffi::CStr;
    
    if app_data_path.is_null() {
        return -1;
    }
    let path = match unsafe { CStr::from_ptr(app_data_path) }.to_str() {
        Ok(s) => s,
        Err(_) => return -1,
    };
    
    match secure_storage::init_secure_storage(path) {
        Ok(_) => 0,
        Err(_) => -1,
    }
    
    /// 使用 ASM 私有混淆加密数据（原始字节）
    ///
    /// # 参数
    /// - `data_base64`: 输入数据的 Base64 编码
    /// - `key_hex`: 512 位密钥的十六进制字符串（128 hex 字符）
    ///
    /// # 返回
    /// Base64 编码的混淆后数据（通过 rust_client_free_string 释放）
    #[no_mangle]
    pub extern "C" fn rust_client_obfuscate(
        data_base64: *const c_char,
        key_hex: *const c_char,
    ) -> *mut c_char {
        crate::crypto::rust_client_obfuscate_message(data_base64, key_hex)
    }
    
    /// 使用 ASM 私有混淆解密数据（原始字节）
    ///
    /// # 参数
    /// - `data_base64`: 混淆后数据的 Base64 编码
    /// - `key_hex`: 512 位密钥的十六进制字符串（128 hex 字符）
    ///
    /// # 返回
    /// Base64 编码的原始数据（通过 rust_client_free_string 释放）
    #[no_mangle]
    pub extern "C" fn rust_client_deobfuscate(
        data_base64: *const c_char,
        key_hex: *const c_char,
    ) -> *mut c_char {
        crate::crypto::rust_client_deobfuscate_message(data_base64, key_hex)
    }
}

/// 释放 Rust 分配的字符串
#[no_mangle]
pub extern "C" fn rust_client_free_string(s: *mut c_char) {
    if !s.is_null() {
        unsafe {
            let _ = CString::from_raw(s);
        }
    }
}

/// 获取客户端安全模块版本
#[no_mangle]
pub extern "C" fn rust_client_version() -> *mut c_char {
    CString::new("0.1.0").unwrap_or_default().into_raw()
}
