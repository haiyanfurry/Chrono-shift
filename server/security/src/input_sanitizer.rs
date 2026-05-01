//! 输入净化模块
//!
//! 对应 C++ [`SecurityManager::InputSanitizer`](server/src/security/SecurityManager.h:65)
//! 无状态纯函数，所有方法均为线程安全
//!
//! # 函数列表
//! - `sanitize_username`: 字母数字 + `_` `-`，最长 32 字符
//! - `sanitize_display_name`: 可打印字符（排除 HTML 特殊字符），最长 64 字符
//! - `sanitize_message`: HTML 转义
//! - `check_password_strength`: 密码强度校验
//! - `is_valid_email`: 邮箱格式校验
//! - `escape_html`: HTML 实体转义

/// 密码强度等级
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum PasswordStrength {
    /// 密码为空
    Empty,
    /// 太短（< 8 字符）
    TooShort,
    /// 太长（> 128 字符）
    TooLong,
    /// 缺少大写字母
    MissingUpper,
    /// 缺少小写字母
    MissingLower,
    /// 缺少数字
    MissingDigit,
    /// 缺少特殊字符
    MissingSpecial,
    /// 有效
    Valid,
}

impl PasswordStrength {
    pub fn is_valid(&self) -> bool {
        matches!(self, PasswordStrength::Valid)
    }

    /// 获取错误描述（用于日志记录）
    pub fn description(&self) -> &'static str {
        match self {
            PasswordStrength::Empty => "密码为空",
            PasswordStrength::TooShort => "密码太短（至少 8 个字符）",
            PasswordStrength::TooLong => "密码太长（最多 128 个字符）",
            PasswordStrength::MissingUpper => "密码需要包含大写字母",
            PasswordStrength::MissingLower => "密码需要包含小写字母",
            PasswordStrength::MissingDigit => "密码需要包含数字",
            PasswordStrength::MissingSpecial => "密码需要包含特殊字符",
            PasswordStrength::Valid => "密码强度有效",
        }
    }
}

/// 净化用户名
///
/// 规则：仅允许字母数字 + `_` + `-`，最长 32 字符
/// 返回 `None` 表示用户名无效
pub fn sanitize_username(input: &str) -> Option<String> {
    let trimmed = input.trim();
    if trimmed.is_empty() || trimmed.len() > 32 {
        return None;
    }
    let sanitized: String = trimmed
        .chars()
        .filter(|c| c.is_alphanumeric() || *c == '_' || *c == '-')
        .collect();
    if sanitized.is_empty() || sanitized.len() > 32 {
        None
    } else {
        Some(sanitized)
    }
}

/// 净化显示名称
///
/// 规则：仅允许可打印字符，排除 `<` `>` `&` `\"` `'`，最长 64 字符
/// 返回 `None` 表示名称无效
pub fn sanitize_display_name(input: &str) -> Option<String> {
    let trimmed = input.trim();
    if trimmed.is_empty() || trimmed.len() > 64 {
        return None;
    }
    let sanitized: String = trimmed
        .chars()
        .filter(|c| {
            c.is_ascii_graphic()
                || *c == ' '
                || *c == '\t'
                || ('\u{00A0}'..='\u{10FFFF}').contains(c) // 允许 Unicode 可打印
        })
        .filter(|c| !matches!(c, '<' | '>' | '&' | '"' | '\''))
        .collect();
    if sanitized.is_empty() || sanitized.len() > 64 {
        None
    } else {
        Some(sanitized)
    }
}

/// 净化消息（直接 HTML 转义）
pub fn sanitize_message(input: &str) -> String {
    escape_html(input)
}

/// 检查密码强度
///
/// # 规则
/// - 长度 8-128 字符
/// - 至少一个大写字母
/// - 至少一个小写字母
/// - 至少一个数字
/// - 至少一个特殊字符
pub fn check_password_strength(password: &str) -> PasswordStrength {
    if password.is_empty() {
        return PasswordStrength::Empty;
    }
    if password.len() < 8 {
        return PasswordStrength::TooShort;
    }
    if password.len() > 128 {
        return PasswordStrength::TooLong;
    }

    let mut has_upper = false;
    let mut has_lower = false;
    let mut has_digit = false;
    let mut has_special = false;

    for c in password.chars() {
        if c.is_ascii_uppercase() {
            has_upper = true;
        } else if c.is_ascii_lowercase() {
            has_lower = true;
        } else if c.is_ascii_digit() {
            has_digit = true;
        } else {
            has_special = true;
        }
    }

    if !has_upper {
        return PasswordStrength::MissingUpper;
    }
    if !has_lower {
        return PasswordStrength::MissingLower;
    }
    if !has_digit {
        return PasswordStrength::MissingDigit;
    }
    if !has_special {
        return PasswordStrength::MissingSpecial;
    }

    PasswordStrength::Valid
}

/// 验证邮箱格式
///
/// 规则：`^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$`
/// 不使用 regex 库，手动解析以保证轻量
pub fn is_valid_email(email: &str) -> bool {
    let email = email.trim();

    if email.is_empty() || email.len() > 254 {
        return false;
    }

    // 查找 @ 分隔符
    let at_pos = match email.rfind('@') {
        Some(pos) => pos,
        None => return false,
    };

    if at_pos == 0 || at_pos >= email.len() - 1 {
        return false;
    }

    let local = &email[..at_pos];
    let domain = &email[at_pos + 1..];

    // 检查 local part: 字母数字 ._%+-
    if local.is_empty() || local.len() > 64 {
        return false;
    }
    for c in local.chars() {
        if !c.is_ascii_alphanumeric() && !matches!(c, '.' | '_' | '%' | '+' | '-') {
            return false;
        }
    }
    // local part 不能以点开头或结尾
    if local.starts_with('.') || local.ends_with('.') {
        return false;
    }

    // 检查 domain part
    if domain.is_empty() || domain.len() > 253 {
        return false;
    }
    let dot_pos = match domain.rfind('.') {
        Some(pos) => pos,
        None => return false,
    };
    if dot_pos == 0 || dot_pos >= domain.len() - 1 {
        return false;
    }
    let tld = &domain[dot_pos + 1..];
    // TLD 至少 2 个字母
    if tld.len() < 2 || !tld.chars().all(|c| c.is_ascii_alphabetic()) {
        return false;
    }
    // domain 标签仅允许字母数字和 -
    for c in domain.chars() {
        if !c.is_ascii_alphanumeric() && c != '.' && c != '-' {
            return false;
        }
    }

    true
}

/// HTML 实体转义
///
/// 替换规则:
/// - `&` → `&`
/// - `<` → `<`
/// - `>` → `>`
/// - `"` → `"`
/// - `'` → `'`
#[allow(unused_assignments)]
pub fn escape_html(input: &str) -> String {
    let mut result = String::with_capacity(input.len() + 32);
    for c in input.chars() {
        match c {
            '&' => result.push_str("\u{0026}amp;"),
            '<' => result.push_str("\u{0026}lt;"),
            '>' => result.push_str("\u{0026}gt;"),
            '"' => result.push_str("\u{0026}quot;"),
            '\'' => result.push_str("\u{0026}apos;"),
            _ => result.push(c),
        }
    }
    result
}

// ============================================================
// FFI 导出
// ============================================================

use std::ffi::CStr;
use std::os::raw::c_char;

/// C FFI: 净化用户名
/// @param input 输入字符串
/// @return 分配的净化后字符串，需要调用 rust_free_string 释放；无效返回 null
#[no_mangle]
pub extern "C" fn rust_sanitize_username(input: *const c_char) -> *mut c_char {
    if input.is_null() {
        return std::ptr::null_mut();
    }
    let input_str = match unsafe { CStr::from_ptr(input) }.to_str() {
        Ok(s) => s,
        Err(_) => return std::ptr::null_mut(),
    };
    match sanitize_username(input_str) {
        Some(s) => {
            let c_str = std::ffi::CString::new(s).unwrap();
            c_str.into_raw()
        }
        None => std::ptr::null_mut(),
    }
}

/// C FFI: 净化显示名称
/// @param input 输入字符串
/// @return 分配的净化后字符串，需要调用 rust_free_string 释放；无效返回 null
#[no_mangle]
pub extern "C" fn rust_sanitize_display_name(input: *const c_char) -> *mut c_char {
    if input.is_null() {
        return std::ptr::null_mut();
    }
    let input_str = match unsafe { CStr::from_ptr(input) }.to_str() {
        Ok(s) => s,
        Err(_) => return std::ptr::null_mut(),
    };
    match sanitize_display_name(input_str) {
        Some(s) => {
            let c_str = std::ffi::CString::new(s).unwrap();
            c_str.into_raw()
        }
        None => std::ptr::null_mut(),
    }
}

/// C FFI: 净化消息（HTML 转义）
/// @param input 输入字符串
/// @return 分配的转义后字符串，需要调用 rust_free_string 释放
#[no_mangle]
pub extern "C" fn rust_sanitize_message(input: *const c_char) -> *mut c_char {
    if input.is_null() {
        return std::ptr::null_mut();
    }
    let input_str = match unsafe { CStr::from_ptr(input) }.to_str() {
        Ok(s) => s,
        Err(_) => return std::ptr::null_mut(),
    };
    let result = sanitize_message(input_str);
    let c_str = std::ffi::CString::new(result).unwrap();
    c_str.into_raw()
}

/// C FFI: 检查密码强度
/// @param password 密码字符串
/// @return 0=Valid, 1=Empty, 2=TooShort, 3=TooLong, 4=MissingUpper, 5=MissingLower, 6=MissingDigit, 7=MissingSpecial
#[no_mangle]
pub extern "C" fn rust_check_password_strength(password: *const c_char) -> i32 {
    if password.is_null() {
        return PasswordStrength::Empty as i32;
    }
    let pwd = match unsafe { CStr::from_ptr(password) }.to_str() {
        Ok(s) => s,
        Err(_) => return PasswordStrength::Empty as i32,
    };
    match check_password_strength(pwd) {
        PasswordStrength::Valid => 0,
        PasswordStrength::Empty => 1,
        PasswordStrength::TooShort => 2,
        PasswordStrength::TooLong => 3,
        PasswordStrength::MissingUpper => 4,
        PasswordStrength::MissingLower => 5,
        PasswordStrength::MissingDigit => 6,
        PasswordStrength::MissingSpecial => 7,
    }
}

/// C FFI: 验证邮箱格式
/// @param email 邮箱字符串
/// @return 1=有效, 0=无效
#[no_mangle]
pub extern "C" fn rust_is_valid_email(email: *const c_char) -> i32 {
    if email.is_null() {
        return 0;
    }
    let email_str = match unsafe { CStr::from_ptr(email) }.to_str() {
        Ok(s) => s,
        Err(_) => return 0,
    };
    if is_valid_email(email_str) { 1 } else { 0 }
}

/// C FFI: HTML 实体转义
/// @param input 输入字符串
/// @return 分配的转义字符串，需要调用 rust_free_string 释放
#[no_mangle]
pub extern "C" fn rust_escape_html(input: *const c_char) -> *mut c_char {
    if input.is_null() {
        return std::ptr::null_mut();
    }
    let input_str = match unsafe { CStr::from_ptr(input) }.to_str() {
        Ok(s) => s,
        Err(_) => return std::ptr::null_mut(),
    };
    let result = escape_html(input_str);
    let c_str = std::ffi::CString::new(result).unwrap();
    c_str.into_raw()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_sanitize_username_valid() {
        assert_eq!(sanitize_username("user_name-123"), Some("user_name-123".into()));
    }

    #[test]
    fn test_sanitize_username_strips_html() {
        assert_eq!(sanitize_username("<script>"), Some("script".into()));
    }

    #[test]
    fn test_sanitize_username_too_long() {
        let long = "a".repeat(33);
        assert_eq!(sanitize_username(&long), None);
    }

    #[test]
    fn test_sanitize_username_empty() {
        assert_eq!(sanitize_username(""), None);
    }

    #[test]
    fn test_sanitize_display_name_valid() {
        assert_eq!(sanitize_display_name("Hello World"), Some("Hello World".into()));
    }

    #[test]
    fn test_sanitize_display_name_strips_html() {
        assert_eq!(sanitize_display_name("<b>bold</b>"), Some("bbold/b".into()));
    }

    #[test]
    fn test_sanitize_display_name_too_long() {
        let long = "a".repeat(65);
        assert_eq!(sanitize_display_name(&long), None);
    }

    #[test]
    fn test_check_password_strength_valid() {
        assert!(check_password_strength("Abcdef1!").is_valid());
    }

    #[test]
    fn test_check_password_strength_too_short() {
        assert_eq!(check_password_strength("Ab1!"), PasswordStrength::TooShort);
    }

    #[test]
    fn test_check_password_strength_no_upper() {
        assert_eq!(check_password_strength("abcdef1!@"), PasswordStrength::MissingUpper);
    }

    #[test]
    fn test_check_password_strength_no_lower() {
        assert_eq!(check_password_strength("ABCDEF1!@"), PasswordStrength::MissingLower);
    }

    #[test]
    fn test_check_password_strength_no_digit() {
        assert_eq!(check_password_strength("Abcdefg!@"), PasswordStrength::MissingDigit);
    }

    #[test]
    fn test_check_password_strength_no_special() {
        assert_eq!(check_password_strength("Abcdefg1"), PasswordStrength::MissingSpecial);
    }

    #[test]
    fn test_is_valid_email_ok() {
        assert!(is_valid_email("user@example.com"));
        assert!(is_valid_email("user.name+tag@example.co.uk"));
    }

    #[test]
    fn test_is_valid_email_invalid() {
        assert!(!is_valid_email("not-an-email"));
        assert!(!is_valid_email("user@"));
        assert!(!is_valid_email("@domain.com"));
        assert!(!is_valid_email("user@.com"));
        assert!(!is_valid_email("user@domain.c"));
    }

    #[test]
    fn test_escape_html() {
        let result = escape_html("<>&\"'");
        assert_eq!(result, "\u{0026}lt;\u{0026}gt;\u{0026}amp;\u{0026}quot;\u{0026}apos;");
        assert_eq!(escape_html("normal text"), "normal text");
    }

    #[test]
    fn test_sanitize_message_escapes_html() {
        let input = "<script>alert('xss')</script>";
        let expected = "\u{0026}lt;script\u{0026}gt;alert(\u{0026}apos;xss\u{0026}apos;)\u{0026}lt;/script\u{0026}gt;";
        assert_eq!(sanitize_message(input), expected);
    }
}
