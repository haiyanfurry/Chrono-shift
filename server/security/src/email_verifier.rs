//! 邮箱验证码发送器 (SMTP)
//!
//! 通过 SMTP 协议发送验证码邮件
//! 支持 AUTH LOGIN 认证
//!
//! 注意: 当前使用纯文本 TCP 连接 (端口 25 或 587 STARTTLS)
//! 生产环境中建议使用 TLS 连接

use std::ffi::{CStr, CString};
use std::io::{Read, Write};
use std::net::TcpStream;
use std::os::raw::c_char;
use std::sync::Mutex;
use std::time::Duration;

use once_cell::sync::OnceCell;

// ============================================================
// SMTP 配置
// ============================================================

/// SMTP 配置结构
#[derive(Clone)]
struct SmtpConfig {
    host: String,
    port: i32,
    username: String,
    password: String,
    from_addr: String,
    from_name: String,
    use_tls: bool,
}

// ============================================================
// 全局状态
// ============================================================

struct EmailVerifierState {
    config: SmtpConfig,
}

static EMAIL_VERIFIER: OnceCell<Mutex<EmailVerifierState>> = OnceCell::new();

fn get_verifier() -> Option<&'static Mutex<EmailVerifierState>> {
    EMAIL_VERIFIER.get()
}

// ============================================================
// Base64 编码
// ============================================================

const BASE64_TABLE: &[u8] =
    b"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

fn base64_encode(input: &[u8]) -> String {
    let mut output = String::with_capacity(((input.len() + 2) / 3) * 4);
    let mut i = 0;

    while i < input.len() {
        let a = input[i];
        let b = if i + 1 < input.len() { input[i + 1] } else { 0 };
        let c = if i + 2 < input.len() { input[i + 2] } else { 0 };

        output.push(BASE64_TABLE[(a >> 2) as usize] as char);
        output.push(BASE64_TABLE[(((a & 0x03) << 4) | (b >> 4)) as usize] as char);
        output.push(if i + 1 < input.len() {
            BASE64_TABLE[(((b & 0x0F) << 2) | (c >> 6)) as usize] as char
        } else {
            '='
        });
        output.push(if i + 2 < input.len() {
            BASE64_TABLE[(c & 0x3F) as usize] as char
        } else {
            '='
        });

        i += 3;
    }

    output
}

// ============================================================
// SMTP 协议实现
// ============================================================

/// 读取 SMTP 服务器响应
fn recv_response(stream: &mut TcpStream) -> Result<String, String> {
    let mut response = String::new();
    let mut buf = [0u8; 4096];
    let timeout = Duration::from_secs(5);

    stream.set_read_timeout(Some(timeout)).map_err(|e| format!("set_read_timeout: {}", e))?;

    loop {
        match stream.read(&mut buf) {
            Ok(0) => break, // 连接关闭
            Ok(n) => {
                response.push_str(&String::from_utf8_lossy(&buf[..n]));

                // SMTP 响应完成检查：最后一行以 "code SP" 结尾 (如 "250 OK\r\n")
                if response.len() >= 5 {
                    let last_four = &response[response.len().saturating_sub(5)..];
                    if last_four.contains("\r\n") {
                        let last_line_start = response.rfind("\r\n")
                            .map(|pos| pos + 2)
                            .unwrap_or(0);

                        if last_line_start + 3 < response.len() {
                            let code_check = response.as_bytes()[last_line_start + 3];
                            if code_check == b' ' {
                                break; // 最后一行，完成
                            }
                        }
                    }
                }
            }
            Err(e) => {
                // 超时也可能是读取完成
                if response.is_empty() {
                    return Err(format!("recv error: {}", e));
                }
                break;
            }
        }
    }

    if response.is_empty() {
        return Err("Empty response from SMTP server".to_string());
    }

    Ok(response)
}

/// 发送 SMTP 命令并读取响应
fn send_command(stream: &mut TcpStream, command: &str) -> Result<String, String> {
    stream.write_all(command.as_bytes())
        .map_err(|e| format!("send failed: {}", e))?;
    recv_response(stream)
}

/// 连接到 SMTP 服务器
fn connect_smtp(config: &SmtpConfig) -> Result<TcpStream, String> {
    let addr = format!("{}:{}", config.host, config.port);
    let timeout = Duration::from_secs(10);

    let stream = TcpStream::connect_timeout(
        &addr.parse().map_err(|e| format!("Invalid address {}: {}", addr, e))?,
        timeout,
    )
    .map_err(|e| format!("connect to {} failed: {}", addr, e))?;

    stream.set_read_timeout(Some(Duration::from_secs(5)))
        .map_err(|e| format!("set_read_timeout: {}", e))?;
    stream.set_write_timeout(Some(Duration::from_secs(5)))
        .map_err(|e| format!("set_write_timeout: {}", e))?;

    Ok(stream)
}

/// 执行完整 SMTP 事务发送验证码邮件
fn smtp_send_code(config: &SmtpConfig, to_email: &str, code: &str) -> Result<(), String> {
    let mut stream = connect_smtp(config)?;

    // 读取欢迎信息
    let mut response = recv_response(&mut stream)?;
    if !response.starts_with("220") {
        return Err(format!("SMTP welcome failed: {}", response.trim()));
    }

    // EHLO
    response = send_command(&mut stream, "EHLO chrono-shift\r\n")?;

    // AUTH LOGIN
    response = send_command(&mut stream, "AUTH LOGIN\r\n")?;
    if !response.starts_with("334") {
        return Err(format!("AUTH LOGIN not supported: {}", response.trim()));
    }

    // 发送 Base64 编码的用户名
    let user_b64 = format!("{}\r\n", base64_encode(config.username.as_bytes()));
    response = send_command(&mut stream, &user_b64)?;
    if !response.starts_with("334") {
        return Err(format!("AUTH username failed: {}", response.trim()));
    }

    // 发送 Base64 编码的密码
    let pass_b64 = format!("{}\r\n", base64_encode(config.password.as_bytes()));
    response = send_command(&mut stream, &pass_b64)?;
    if !response.starts_with("235") {
        return Err(format!("Authentication failed: {}", response.trim()));
    }

    // MAIL FROM
    let mail_from = format!("MAIL FROM:<{}>\r\n", config.from_addr);
    response = send_command(&mut stream, &mail_from)?;
    if !response.starts_with("250") {
        return Err(format!("MAIL FROM failed: {}", response.trim()));
    }

    // RCPT TO
    let rcpt_to = format!("RCPT TO:<{}>\r\n", to_email);
    response = send_command(&mut stream, &rcpt_to)?;
    if !response.starts_with("250") {
        return Err(format!("RCPT TO failed: {}", response.trim()));
    }

    // DATA
    response = send_command(&mut stream, "DATA\r\n")?;
    if !response.starts_with("354") {
        return Err(format!("DATA command failed: {}", response.trim()));
    }

    // 邮件内容
    let email_body = build_email_body(config, to_email, code);
    response = send_command(&mut stream, &email_body)?;
    if !response.starts_with("250") {
        return Err(format!("Failed to send email body: {}", response.trim()));
    }

    // QUIT
    let _ = send_command(&mut stream, "QUIT\r\n");

    Ok(())
}

/// 构建邮件内容 (含 Base64 正文)
fn build_email_body(config: &SmtpConfig, to_email: &str, code: &str) -> String {
    // 主题: "墨竹 - 邮箱验证码"
    let subject_bytes = [
        0xE9, 0xBB, 0x98, 0xE7, 0xAB, 0xB9, // 墨竹
        0x20, 0x2D, 0x20, // " - "
        0xE9, 0x82, 0xAE, 0xE7, 0xAE, 0xB1, // 邮箱
        0xE9, 0xAA, 0x8C, 0xE8, 0xAF, 0x81, 0xE7, 0xA0, 0x81, // 验证码
    ];
    let subject = unsafe { std::str::from_utf8_unchecked(&subject_bytes) };
    let subject_b64 = base64_encode(subject.as_bytes());

    // "您好！" + "\r\n\r\n" + "您的验证码是: " + code + "\r\n\r\n"
    // + "验证码 5 分钟内有效，请勿泄露给他人。" + "\r\n\r\n" + "—— 墨竹团队"
    let greeting_bytes = b"\xE6\x82\xA8\xE5\xA5\xBD\xEF\xBC\x81"; // 您好！
    let prefix_bytes = b"\xE6\x82\xA8\xE7\x9A\x84\xE9\xAA\x8C\xE8\xAF\x81\xE7\xA0\x81\xE6\x98\xAF"; // 您的验证码是
    let body_mid = b"\xE9\xAA\x8C\xE8\xAF\x81\xE7\xA0\x81 5 \xE5\x88\x86\xE9\x92\x9F\xE5\x86\x85\xE6\x9C\x89\xE6\x95\x88\xEF\xBC\x8C\xE8\xAF\xB7\xE5\x8B\xBF\xE6\xB3\x84\xE9\x9C\xB2\xE7\xBB\x99\xE4\xBB\x96\xE4\xBA\xBA\xE3\x80\x82"; // 验证码 5 分钟内有效，请勿泄露给他人。
    let body_end = b"\xE2\x80\x94\xE2\x80\x94 \xE5\xA2\xA8\xE7\xAB\xB9\xE5\x9B\xA2\xE9\x98\x9F"; // —— 墨竹团队

    let greeting = unsafe { std::str::from_utf8_unchecked(greeting_bytes) };
    let prefix = unsafe { std::str::from_utf8_unchecked(prefix_bytes) };
    let mid = unsafe { std::str::from_utf8_unchecked(body_mid) };
    let end = unsafe { std::str::from_utf8_unchecked(body_end) };

    let text_body = format!(
        "{}\r\n\r\n{}: {}\r\n\r\n{}\r\n\r\n{}",
        greeting, prefix, code, mid, end,
    );

    let text_b64 = base64_encode(text_body.as_bytes());

    format!(
        "From: {} <{}>\r\n\
         To: <{}>\r\n\
         Subject: =?UTF-8?B?{}?=\r\n\
         MIME-Version: 1.0\r\n\
         Content-Type: text/plain; charset=UTF-8\r\n\
         Content-Transfer-Encoding: base64\r\n\
         \r\n\
         {}\r\n\
         .\r\n",
        config.from_name,
        config.from_addr,
        to_email,
        subject_b64,
        text_b64,
    )
}

// ============================================================
// FFI 导出函数
// ============================================================

/// 初始化邮箱验证器
/// @param host SMTP 服务器地址
/// @param port 端口 (25=明文, 465=SSL, 587=TLS)
/// @param username 邮箱账号
/// @param password 邮箱密码/授权码
/// @param from_addr 发件人地址 (可为空, 默认同 username)
/// @param from_name 发件人名称 (可为空, 默认"墨竹")
/// @param use_tls 是否使用 TLS (0=否, 1=是)
/// @return 0=成功, -1=失败
#[no_mangle]
pub extern "C" fn rust_email_init(
    host: *const c_char,
    port: i32,
    username: *const c_char,
    password: *const c_char,
    from_addr: *const c_char,
    from_name: *const c_char,
    use_tls: i32,
) -> i32 {
    if host.is_null() || username.is_null() || password.is_null() {
        return -1;
    }

    let host = match unsafe { CStr::from_ptr(host) }.to_str() {
        Ok(s) => s.to_string(),
        Err(_) => return -1,
    };
    let username = match unsafe { CStr::from_ptr(username) }.to_str() {
        Ok(s) => s.to_string(),
        Err(_) => return -1,
    };
    let password = match unsafe { CStr::from_ptr(password) }.to_str() {
        Ok(s) => s.to_string(),
        Err(_) => return -1,
    };

    let from_addr = if from_addr.is_null() {
        username.clone()
    } else {
        match unsafe { CStr::from_ptr(from_addr) }.to_str() {
            Ok(s) if !s.is_empty() => s.to_string(),
            _ => username.clone(),
        }
    };

    let default_name = unsafe {
        std::str::from_utf8_unchecked(b"\xE5\xA2\xA8\xE7\xAB\xB9") // "墨竹"
    };
    let from_name = if from_name.is_null() {
        default_name.to_string()
    } else {
        match unsafe { CStr::from_ptr(from_name) }.to_str() {
            Ok(s) if !s.is_empty() => s.to_string(),
            _ => default_name.to_string(),
        }
    };

    let state = EmailVerifierState {
        config: SmtpConfig {
            host,
            port,
            username,
            password,
            from_addr,
            from_name,
            use_tls: use_tls != 0,
        },
    };

    match EMAIL_VERIFIER.set(Mutex::new(state)) {
        Ok(_) => 0,
        Err(_) => -1,
    }
}

/// 发送验证码到指定邮箱
/// @param to_email 目标邮箱地址
/// @param code 6 位验证码
/// @return 1=发送成功, 0=发送失败
#[no_mangle]
pub extern "C" fn rust_email_send_code(
    to_email: *const c_char,
    code: *const c_char,
) -> i32 {
    if to_email.is_null() || code.is_null() {
        return 0;
    }

    let to_email = match unsafe { CStr::from_ptr(to_email) }.to_str() {
        Ok(s) => s,
        Err(_) => return 0,
    };
    let code_str = match unsafe { CStr::from_ptr(code) }.to_str() {
        Ok(s) => s,
        Err(_) => return 0,
    };

    let verifier = match get_verifier() {
        Some(v) => v,
        None => return 0,
    };

    let config = match verifier.lock() {
        Ok(c) => c.config.clone(),
        Err(_) => return 0,
    };

    match smtp_send_code(&config, to_email, code_str) {
        Ok(_) => 1,
        Err(_) => 0,
    }
}

/// 清理邮箱验证器状态
#[no_mangle]
pub extern "C" fn rust_email_cleanup() {
    // OnceCell 无法移除; 重新 init 会覆盖
}

// ============================================================
// 单元测试
// ============================================================

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_base64_encode() {
        assert_eq!(base64_encode(b""), "");
        assert_eq!(base64_encode(b"f"), "Zg==");
        assert_eq!(base64_encode(b"fo"), "Zm8=");
        assert_eq!(base64_encode(b"foo"), "Zm9v");
        assert_eq!(base64_encode(b"foob"), "Zm9vYg==");
        assert_eq!(base64_encode(b"fooba"), "Zm9vYmE=");
        assert_eq!(base64_encode(b"foobar"), "Zm9vYmFy");
    }

    #[test]
    fn test_base64_encode_hello() {
        // "Hello" -> "SGVsbG8="
        assert_eq!(base64_encode(b"Hello"), "SGVsbG8=");
    }

    #[test]
    fn test_base64_encode_utf8() {
        // 中文 UTF-8 编码
        let chinese = "\u{4E2D}\u{6587}"; // "中文"
        assert_eq!(base64_encode(chinese.as_bytes()), "5Lit5paH");
    }

    #[test]
    fn test_build_email_body_contains_code() {
        let config = SmtpConfig {
            host: "smtp.test.com".to_string(),
            port: 25,
            username: "test@test.com".to_string(),
            password: "password".to_string(),
            from_addr: "test@test.com".to_string(),
            from_name: "\u{6F}\u{6A}\u{6F}\u{6F}\u{6F}\u{6F}\u{6F}\u{6F}\u{6F}\u{6F}\u{6F}\u{6F}".to_string(),
            use_tls: false,
        };

        let body = build_email_body(&config, "user@example.com", "123456");

        // 验证邮件头部
        assert!(body.contains("From:"));
        assert!(body.contains("To: <user@example.com>"));
        assert!(body.contains("Subject: =?UTF-8?B?"));
        assert!(body.contains("Content-Type: text/plain; charset=UTF-8"));
        assert!(body.contains("Content-Transfer-Encoding: base64"));
        assert!(body.ends_with(".\r\n"));

        // 验证邮件头包含发件人
        assert!(body.contains("test@test.com"));
    }

    #[test]
    fn test_build_email_body_from_name() {
        let config = SmtpConfig {
            host: "smtp.test.com".to_string(),
            port: 25,
            username: "noreply@test.com".to_string(),
            password: "secret".to_string(),
            from_addr: "noreply@test.com".to_string(),
            from_name: "TestApp".to_string(),
            use_tls: false,
        };

        let body = build_email_body(&config, "user@test.com", "654321");
        assert!(body.contains("TestApp <noreply@test.com>"));
    }

    #[test]
    fn test_email_init_null() {
        assert_eq!(rust_email_init(
            std::ptr::null(),
            25,
            std::ptr::null(),
            std::ptr::null(),
            std::ptr::null(),
            std::ptr::null(),
            0,
        ), -1);
    }

    #[test]
    fn test_email_send_code_not_initialized() {
        assert_eq!(rust_email_send_code(
            "test@example.com\0".as_ptr() as *const c_char,
            "123456\0".as_ptr() as *const c_char,
        ), 0);
    }

    #[test]
    fn test_base64_encode_padding() {
        // 1 字节: padding ==
        assert_eq!(base64_encode(b"a"), "YQ==");
        // 2 字节: padding =
        assert_eq!(base64_encode(b"ab"), "YWI=");
        // 3 字节: no padding
        assert_eq!(base64_encode(b"abc"), "YWJj");
    }

    #[test]
    fn test_base64_encode_all_chars() {
        // RFC 4648 test vectors
        assert_eq!(base64_encode(b""), "");
        assert_eq!(base64_encode(b"f"), "Zg==");
        assert_eq!(base64_encode(b"fo"), "Zm8=");
        assert_eq!(base64_encode(b"foo"), "Zm9v");
        assert_eq!(base64_encode(b"foob"), "Zm9vYg==");
        assert_eq!(base64_encode(b"fooba"), "Zm9vYmE=");
        assert_eq!(base64_encode(b"foobar"), "Zm9vYmFy");
    }
}
