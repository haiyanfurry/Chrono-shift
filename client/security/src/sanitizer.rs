//! 输入安全校验模块
//! 提供路径、用户名、UID、Token 等安全校验函数

use std::path::Path;

/// 检查路径是否在安全范围内（禁止路径穿越）
pub fn is_safe_path(path: &str) -> bool {
    let p = Path::new(path);
    // 禁止包含 .. 路径穿越
    if p.components().any(|c| c.as_os_str() == "..") {
        return false;
    }
    // 禁止绝对路径
    if p.is_absolute() || path.starts_with('/') || path.starts_with('\\') {
        return false;
    }
    // 禁止包含空字节
    if path.contains('\0') {
        return false;
    }
    true
}

/// 净化文件名，移除危险字符
pub fn sanitize_filename(name: &str) -> String {
    let dangerous = ['/', '\\', ':', '*', '?', '"', '<', '>', '|', '\0'];
    name.chars()
        .map(|c| if dangerous.contains(&c) { '_' } else { c })
        .collect()
}

/// 验证用户名字符是否安全（仅允许字母、数字、下划线、中划线、中文）
pub fn validate_username(name: &str) -> bool {
    if name.is_empty() || name.len() > 64 {
        return false;
    }
    name.chars().all(|c| {
        c.is_alphanumeric() || c == '_' || c == '-' || c == '.' || c >= '\u{4e00}'
    })
}

/// 验证 UID 是否安全（仅允许字母数字下划线）
pub fn validate_uid(uid: &str) -> bool {
    if uid.is_empty() || uid.len() > 64 {
        return false;
    }
    uid.chars().all(|c| c.is_alphanumeric() || c == '_' || c == '-')
}

/// 验证 Token 是否安全（仅允许字母数字和常见 token 字符）
pub fn validate_token(token: &str) -> bool {
    if token.is_empty() || token.len() > 512 {
        return false;
    }
    token.chars().all(|c| c.is_alphanumeric() || c == '_' || c == '-' || c == '.')
}

/// 验证消息长度是否在安全范围内
pub fn validate_message_length(text: &str) -> bool {
    if text.is_empty() {
        return false;
    }
    if text.len() > 1_048_576 {
        // 最大 1MB
        return false;
    }
    true
}
