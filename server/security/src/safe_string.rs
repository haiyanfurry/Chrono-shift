//! safe_string — Rust 安全字符串校验模块
//!
//! # 设计目标
//! 针对 Java UTF-16 高位截断漏洞提供强制安全校验。
//! 同时提供 C ABI (`extern "C"`) 和 JNI (`extern "system"`) 两种 FFI 接口。
//!
//! # 核心原则
//! - Java TrafficFilter 可能被绕过（UTF-16 代理对截断）
//! - Rust safe_string 是 **唯一的安全仲裁者**
//! - 只有 Rust 说"安全"，请求才放行
//!
//! # 校验项
//! 1. **空字节注入** — `\0` 在 C/C++ 中截断字符串
//! 2. **代理对完整性** — UTF-8 中不应出现 UTF-16 代理对码位 (0xD800-0xDFFF)
//! 3. **编码一致性** — byte_len 与 char_count 的合理比例
//! 4. **控制字符** — 除 \n \r \t 外的控制字符

use std::ffi::{CStr, CString};
use std::os::raw::c_char;

// ============================================================
// 校验结果枚举
// ============================================================

/// 校验结果
///
/// 使用 `#[repr(i32)]` 确保 C/C++/Java 端可以直接使用整数值
#[repr(i32)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ValidationResult {
    /// 字符串安全
    Safe = 0,
    /// 空字节注入 (\\0)
    NullByteInjection = -1,
    /// 孤立的代理对 (lone surrogate 0xD800-0xDFFF)
    LoneSurrogate = -2,
    /// 可疑编码 (byte_len / char_count 比例异常)
    SuspiciousEncoding = -3,
    /// 非法控制字符
    ControlCharacter = -4,
    /// 非法 UTF-8 序列
    InvalidUtf8 = -5,
}

impl ValidationResult {
    /// 是否为安全结果
    pub fn is_safe(&self) -> bool {
        matches!(self, ValidationResult::Safe)
    }

    /// 获取错误描述
    pub fn description(&self) -> &'static str {
        match self {
            ValidationResult::Safe => "safe",
            ValidationResult::NullByteInjection => "null byte injection detected",
            ValidationResult::LoneSurrogate => "lone surrogate pair detected",
            ValidationResult::SuspiciousEncoding => "suspicious encoding detected",
            ValidationResult::ControlCharacter => "illegal control character detected",
            ValidationResult::InvalidUtf8 => "invalid UTF-8 sequence",
        }
    }
}

// ============================================================
// 核心校验逻辑
// ============================================================

/// UTF-8 安全字符串校验 (核心逻辑)
///
/// 对输入的字节切片执行四项安全检查：
///
/// 1. **空字节检查** — 检测 `\0` 空字节注入
/// 2. **代理对完整性检查** — 检测孤立的 UTF-16 代理对码位
/// 3. **编码一致性检查** — 验证 byte_len 与 char_count 的合理比例
/// 4. **控制字符检查** — 过滤除 \n \r \t 外的控制字符
///
/// # 参数
/// * `bytes` — 待校验的 UTF-8 字节切片
///
/// # 返回值
/// * `ValidationResult::Safe` — 字符串安全
/// * 其他值 — 对应的错误类型
///
/// # 示例
/// ```
/// use chrono_server_security::safe_string::validate_utf8_safe;
/// use chrono_server_security::safe_string::ValidationResult;
///
/// let safe = validate_utf8_safe(b"hello world");
/// assert_eq!(safe, ValidationResult::Safe);
///
/// let bad = validate_utf8_safe(b"hello\x00world");
/// assert_eq!(bad, ValidationResult::NullByteInjection);
/// ```
pub fn validate_utf8_safe(bytes: &[u8]) -> ValidationResult {
    // 0. 代理对完整性检查 (字节级扫描)
    //    在 from_utf8 之前执行，因为新版 Rust 编译器将代理对视为无效 UTF-8
    //    UTF-8 编码: U+D800-U+DFFF → 0xED [0xA0-0xBF] [0x80-0xBF]
    //    如果出现，说明可能是 Java JNI UTF-16→UTF-8 转换截断导致的残留
    let mut i = 0;
    while i < bytes.len() {
        if bytes[i] == 0xED && i + 2 < bytes.len() {
            let b2 = bytes[i + 1];
            let b3 = bytes[i + 2];
            if (0xA0..=0xBF).contains(&b2) && (0x80..=0xBF).contains(&b3) {
                return ValidationResult::LoneSurrogate;
            }
        }
        // 跳到下一个字符边界
        let c = bytes[i];
        i += if c < 0x80 {
            1
        } else if c < 0xE0 {
            2
        } else if c < 0xF0 {
            3
        } else {
            4
        };
    }

    // 先验证 UTF-8 合法性
    let s = match std::str::from_utf8(bytes) {
        Ok(s) => s,
        Err(_) => return ValidationResult::InvalidUtf8,
    };

    // 1. 空字节检查
    //    \0 在 C/C++ 字符串中会截断字符串，导致安全绕过
    if s.contains('\0') {
        return ValidationResult::NullByteInjection;
    }

    // 2. 编码一致性检查
    //    正常情况下: byte_len ≈ char_count * (1~4)
    //    如果比例异常，说明可能有编码注入
    let byte_len = bytes.len();
    let char_count = s.chars().count();
    if byte_len > char_count * 4 {
        // 字节数远大于字符数 * 4，说明有非最短编码形式
        return ValidationResult::SuspiciousEncoding;
    }
    if byte_len < char_count {
        // 字节数小于字符数，不可能
        return ValidationResult::SuspiciousEncoding;
    }

    // 3. 控制字符检查
    //    允许: \n (0x0A), \r (0x0D), \t (0x09)
    //    拦截: 其他控制字符 (0x00-0x08, 0x0B, 0x0C, 0x0E-0x1F)
    for c in s.chars() {
        let code = c as u32;
        if code < 0x20 && code != 0x09 && code != 0x0A && code != 0x0D {
            return ValidationResult::ControlCharacter;
        }
    }

    ValidationResult::Safe
}

// ============================================================
// C ABI — 供 C++ 服务端中间件链调用
// ============================================================

/// C ABI: 验证 UTF-8 字符串安全性
///
/// # 参数
/// * `input` — 输入字节指针，不可为空
/// * `len` — 输入字节长度
///
/// # 返回值
/// 返回 [`ValidationResult`] 的整数值:
/// * `0` = 安全
/// * `< 0` = 对应的错误码
///
/// # 安全性
/// * `input` 必须指向有效的内存区域，长度至少为 `len`
/// * `input` 必须是已初始化的内存
#[no_mangle]
pub extern "C" fn rust_validate_utf8_safe(
    input: *const u8,
    len: usize,
) -> i32 {
    if input.is_null() {
        return ValidationResult::InvalidUtf8 as i32;
    }
    let bytes = unsafe { std::slice::from_raw_parts(input, len) };
    validate_utf8_safe(bytes) as i32
}

/// C ABI: 获取校验结果描述
///
/// 返回的错误描述字符串需要调用 `rust_free_string` 释放
#[no_mangle]
pub extern "C" fn rust_validation_result_description(result: i32) -> *mut c_char {
    let desc = match result {
        0 => "safe",
        -1 => "null byte injection detected",
        -2 => "lone surrogate pair detected",
        -3 => "suspicious encoding detected",
        -4 => "illegal control character detected",
        -5 => "invalid UTF-8 sequence",
        _ => "unknown validation result",
    };
    match CString::new(desc) {
        Ok(cs) => cs.into_raw(),
        Err(_) => std::ptr::null_mut(),
    }
}

// ============================================================
// JNI — 供 Java TrafficFilter 调用
// ============================================================

#[cfg(feature = "jni")]
pub mod jni {
    use super::*;
    use jni::objects::JClass;
    use jni::sys::{jint, jstring};
    use jni::JNIEnv;

    /// JNI 桥接: Java 端调用 safe_string 校验
    ///
    /// Java 声明:
    /// ```java
    /// public class SafeString {
    ///     public static native int validate(String input);
    /// }
    /// ```
    ///
    /// # 参数
    /// * `input` — Java String 对象，JVM 会自动转换为 UTF-8
    ///
    /// # 返回值
    /// 返回 ValidationResult 的整数值
    ///
    /// # 安全设计
    /// JVM 将 Java UTF-16 String 转换为 modified UTF-8 后传给 JNI
    /// safe_string 在 Rust 侧接收 modified UTF-8 进行深度校验
    /// 如果 Java UTF-16→UTF-8 转换发生截断，Rust 会捕获异常编码
    #[no_mangle]
    pub extern "system" fn Java_com_chronoshift_security_SafeString_validate(
        mut env: JNIEnv,
        _class: JClass,
        input: jstring,
    ) -> jint {
        if input.is_null() {
            return ValidationResult::InvalidUtf8 as jint;
        }

        // 获取 Java String 的 modified UTF-8 字节
        // JNI GetStringUTFChars 返回的是 modified UTF-8 编码
        let bytes = match env.get_string_utf_chars(input) {
            Ok(c_str) => c_str,
            Err(_) => return ValidationResult::InvalidUtf8 as jint,
        };

        // 将 CStr 转换为字节切片
        let slice = bytes.to_bytes();
        validate_utf8_safe(slice) as jint
    }
}

// ============================================================
// 单元测试
// ============================================================

#[cfg(test)]
mod tests {
    use super::*;

    // ----------------------------------------------------------
    // 正常字符串测试
    // ----------------------------------------------------------

    #[test]
    fn test_safe_ascii() {
        assert_eq!(validate_utf8_safe(b"hello world"), ValidationResult::Safe);
    }

    #[test]
    fn test_safe_unicode() {
        assert_eq!(validate_utf8_safe("你好世界".as_bytes()), ValidationResult::Safe);
    }

    #[test]
    fn test_safe_emoji() {
        assert_eq!(validate_utf8_safe("😀🌍🎉".as_bytes()), ValidationResult::Safe);
    }

    #[test]
    fn test_safe_with_newline() {
        assert_eq!(validate_utf8_safe(b"line1\nline2\r\nline3\t"), ValidationResult::Safe);
    }

    #[test]
    fn test_safe_special_chars() {
        assert_eq!(
            validate_utf8_safe(b"test@#$%^&*()_+-=[]{}|;':\",./<>?"),
            ValidationResult::Safe
        );
    }

    #[test]
    fn test_safe_mixed() {
        assert_eq!(
            validate_utf8_safe("Hello 你好 😀\n".as_bytes()),
            ValidationResult::Safe
        );
    }

    #[test]
    fn test_safe_max_length_username() {
        let name = "user_1234_test_abcd".as_bytes();
        assert_eq!(validate_utf8_safe(name), ValidationResult::Safe);
    }

    #[test]
    fn test_safe_email() {
        let email = b"user@example.com";
        assert_eq!(validate_utf8_safe(email), ValidationResult::Safe);
    }

    #[test]
    fn test_safe_jwt() {
        let jwt = b"eyJhbGciOiJSUzI1NiJ9.eyJzdWIiOiIxMjM0NTY3ODkwIn0.signature";
        assert_eq!(validate_utf8_safe(jwt), ValidationResult::Safe);
    }

    // ----------------------------------------------------------
    // 空字节注入测试
    // ----------------------------------------------------------

    #[test]
    fn test_null_byte_injection() {
        assert_eq!(
            validate_utf8_safe(b"admin\0"),
            ValidationResult::NullByteInjection
        );
    }

    #[test]
    fn test_null_byte_middle() {
        assert_eq!(
            validate_utf8_safe(b"SELECT * FROM users WHERE id = 1\0; DROP TABLE users;"),
            ValidationResult::NullByteInjection
        );
    }

    #[test]
    fn test_null_byte_sql_injection() {
        assert_eq!(
            validate_utf8_safe(b"admin\0' OR '1'='1"),
            ValidationResult::NullByteInjection
        );
    }

    #[test]
    fn test_multiple_null_bytes() {
        assert_eq!(
            validate_utf8_safe(b"\0\0\0"),
            ValidationResult::NullByteInjection
        );
    }

    #[test]
    fn test_null_byte_in_unicode() {
        assert_eq!(
            validate_utf8_safe("你好\0世界".as_bytes()),
            ValidationResult::NullByteInjection
        );
    }

    // ----------------------------------------------------------
    // 代理对完整性测试
    // ----------------------------------------------------------

    #[test]
    fn test_lone_high_surrogate() {
        // U+D800 (高位代理对单独出现)
        assert_eq!(
            validate_utf8_safe(&[0xED, 0xA0, 0x80]),
            ValidationResult::LoneSurrogate
        );
    }

    #[test]
    fn test_lone_low_surrogate() {
        // U+DC00 (低位代理对单独出现)
        assert_eq!(
            validate_utf8_safe(&[0xED, 0xB0, 0x80]),
            ValidationResult::LoneSurrogate
        );
    }

    #[test]
    fn test_lone_surrogate_in_string() {
        // 正常字符串 + 代理对
        assert_eq!(
            validate_utf8_safe(&[0x61, 0xED, 0xA0, 0x80, 0x62]),
            ValidationResult::LoneSurrogate
        );
    }

    #[test]
    fn test_all_surrogate_range() {
        // 测试 0xD800-0xDFFF 范围内的各个边界
        for code in [0xD800, 0xDBFF, 0xDC00, 0xDFFF].iter() {
            let utf8_bytes = encode_surrogate(*code);
            assert_eq!(
                validate_utf8_safe(&utf8_bytes),
                ValidationResult::LoneSurrogate,
                "surrogate U+{:X} should be rejected",
                code
            );
        }
    }

    /// 将 UTF-16 代理对码位编码为 UTF-8 字节 (用于测试)
    fn encode_surrogate(code: u32) -> Vec<u8> {
        // UTF-8 编码 surrogate (3 字节): 1110xxxx 10xxxxxx 10xxxxxx
        vec![
            0xE0 | ((code >> 12) as u8 & 0x0F),
            0x80 | ((code >> 6) as u8 & 0x3F),
            0x80 | (code as u8 & 0x3F),
        ]
    }

    // ----------------------------------------------------------
    // 编码一致性测试
    // ----------------------------------------------------------

    #[test]
    fn test_overlong_encoding() {
        // 过长的 ASCII 编码 (2 字节编码 'A')
        // 正常 'A' = 0x41, 过长 = 0xC1 0x81
        let overlong = &[0xC1, 0x81];
        assert_eq!(
            validate_utf8_safe(overlong),
            ValidationResult::InvalidUtf8 // Rust 的 from_utf8 会拒绝过长编码
        );
    }

    #[test]
    fn test_continuation_byte_start() {
        // 以连续字节开头
        let bad = &[0x80, 0x81, 0x82];
        assert_eq!(
            validate_utf8_safe(bad),
            ValidationResult::InvalidUtf8
        );
    }

    #[test]
    fn test_incomplete_sequence() {
        // 不完整的 UTF-8 序列
        let bad = &[0xE4, 0xB8]; // 缺少第三个字节
        assert_eq!(
            validate_utf8_safe(bad),
            ValidationResult::InvalidUtf8
        );
    }

    // ----------------------------------------------------------
    // 控制字符测试
    // ----------------------------------------------------------

    #[test]
    fn test_control_char_bell() {
        assert_eq!(
            validate_utf8_safe(b"hello\x07world"),
            ValidationResult::ControlCharacter
        );
    }

    #[test]
    fn test_control_char_escape() {
        assert_eq!(
            validate_utf8_safe(b"test\x1B"),
            ValidationResult::ControlCharacter
        );
    }

    #[test]
    fn test_control_char_null_is_null_byte() {
        // \0 既是空字节也是控制字符，但空字节检测优先级更高
        assert_eq!(
            validate_utf8_safe(b"\x00"),
            ValidationResult::NullByteInjection
        );
    }

    #[test]
    fn test_allowed_control_chars() {
        // \n \r \t 应该放行
        assert_eq!(validate_utf8_safe(b"\n"), ValidationResult::Safe);
        assert_eq!(validate_utf8_safe(b"\r"), ValidationResult::Safe);
        assert_eq!(validate_utf8_safe(b"\t"), ValidationResult::Safe);
    }

    #[test]
    fn test_disallowed_control_chars() {
        // \b (0x08), \v (0x0B), \f (0x0C) 应该拦截
        assert_eq!(
            validate_utf8_safe(b"\x08"),
            ValidationResult::ControlCharacter
        );
        assert_eq!(
            validate_utf8_safe(b"\x0B"),
            ValidationResult::ControlCharacter
        );
        assert_eq!(
            validate_utf8_safe(b"\x0C"),
            ValidationResult::ControlCharacter
        );
    }

    // ----------------------------------------------------------
    // 边界情况测试
    // ----------------------------------------------------------

    #[test]
    fn test_empty_string() {
        assert_eq!(validate_utf8_safe(b""), ValidationResult::Safe);
    }

    #[test]
    fn test_single_char() {
        assert_eq!(validate_utf8_safe(b"a"), ValidationResult::Safe);
    }

    #[test]
    fn test_max_control_char() {
        // 0x1F (Unit Separator) — 应该拦截
        assert_eq!(
            validate_utf8_safe(b"\x1F"),
            ValidationResult::ControlCharacter
        );
    }

    #[test]
    fn test_sql_injection_keywords() {
        // SQL 关键词本身不是漏洞，只是语法
        // safe_string 不做语义分析，只做编码安全检查
        assert_eq!(
            validate_utf8_safe(b"' OR '1'='1"),
            ValidationResult::Safe
        );
        assert_eq!(
            validate_utf8_safe(b"'; DROP TABLE users; --"),
            ValidationResult::Safe
        );
    }

    #[test]
    fn test_xss_vectors() {
        // XSS 向量在 safe_string 角度只是普通字符
        // (由 Java TrafficFilter 做规则匹配)
        assert_eq!(
            validate_utf8_safe(b"<script>alert('xss')</script>"),
            ValidationResult::Safe
        );
        assert_eq!(
            validate_utf8_safe(b"javascript:void(0)"),
            ValidationResult::Safe
        );
    }

    // ----------------------------------------------------------
    // 性能回归测试
    // ----------------------------------------------------------

    #[test]
    fn test_large_string() {
        let large = vec![b'A'; 100_000];
        assert_eq!(validate_utf8_safe(&large), ValidationResult::Safe);
    }

    #[test]
    fn test_large_string_with_null() {
        let mut large = vec![b'A'; 100_000];
        large[50_000] = b'\0';
        assert_eq!(
            validate_utf8_safe(&large),
            ValidationResult::NullByteInjection
        );
    }

    // ----------------------------------------------------------
    // C ABI 测试
    // ----------------------------------------------------------

    #[test]
    fn test_c_abi_safe() {
        let input = b"hello";
        let result = rust_validate_utf8_safe(input.as_ptr(), input.len());
        assert_eq!(result, ValidationResult::Safe as i32);
    }

    #[test]
    fn test_c_abi_null() {
        let result = rust_validate_utf8_safe(std::ptr::null(), 0);
        assert_eq!(result, ValidationResult::InvalidUtf8 as i32);
    }

    #[test]
    fn test_c_abi_null_byte() {
        let input = b"admin\0extra";
        let result = rust_validate_utf8_safe(input.as_ptr(), input.len());
        assert_eq!(result, ValidationResult::NullByteInjection as i32);
    }

    #[test]
    fn test_c_abi_lone_surrogate() {
        let lone = &[0x61, 0xED, 0xA0, 0x80, 0x62]; // a + U+D800 + b
        let result = rust_validate_utf8_safe(lone.as_ptr(), lone.len());
        assert_eq!(result, ValidationResult::LoneSurrogate as i32);
    }

    #[test]
    fn test_c_abi_empty() {
        let input = b"";
        let result = rust_validate_utf8_safe(input.as_ptr(), input.len());
        assert_eq!(result, ValidationResult::Safe as i32);
    }

    // ----------------------------------------------------------
    // 描述信息测试
    // ----------------------------------------------------------

    #[test]
    fn test_description() {
        assert_eq!(ValidationResult::Safe.description(), "safe");
        assert_eq!(
            ValidationResult::NullByteInjection.description(),
            "null byte injection detected"
        );
        assert_eq!(
            ValidationResult::LoneSurrogate.description(),
            "lone surrogate pair detected"
        );
        assert_eq!(
            ValidationResult::InvalidUtf8.description(),
            "invalid UTF-8 sequence"
        );
    }

    #[test]
    fn test_is_safe() {
        assert!(ValidationResult::Safe.is_safe());
        assert!(!ValidationResult::NullByteInjection.is_safe());
        assert!(!ValidationResult::LoneSurrogate.is_safe());
    }
}
