// client/security/src/asm_bridge.rs
// Rust → ASM FFI 桥接层
// 调用 obfuscate.asm 中的 asm_obfuscate / asm_deobfuscate

/// ASM 加密函数外部声明
///
/// # 参数
/// - `data`: 输入数据指针
/// - `len`: 数据长度
/// - `key`: 64 字节 (512 位) 密钥指针
/// - `out`: 输出缓冲区指针（长度 >= data.len()）
///
/// # 返回值
/// - `0`: 成功
/// - `-1`: 失败
extern "C" {
    fn asm_obfuscate(
        data: *const u8, len: usize,
        key: *const u8, out: *mut u8
    ) -> i32;

    fn asm_deobfuscate(
        data: *const u8, len: usize,
        key: *const u8, out: *mut u8
    ) -> i32;
}

/// 调用 ASM 加密
///
/// # 参数
/// - `data`: 待加密的明文数据
/// - `key`: 64 字节密钥 (512 位)
///
/// # 返回值
/// - `Ok(Vec<u8>)`: 加密后的密文
/// - `Err(String)`: 加密失败原因
pub fn obfuscate(data: &[u8], key: &[u8; 64]) -> Result<Vec<u8>, String> {
    if data.is_empty() {
        return Err("数据为空".into());
    }

    let mut out = vec![0u8; data.len()];
    let ret = unsafe {
        asm_obfuscate(data.as_ptr(), data.len(), key.as_ptr(), out.as_mut_ptr())
    };

    if ret != 0 {
        Err("ASM 加密失败".into())
    } else {
        Ok(out)
    }
}

/// 调用 ASM 解密
///
/// # 参数
/// - `data`: 待解密的密文数据
/// - `key`: 64 字节密钥 (512 位)
///
/// # 返回值
/// - `Ok(Vec<u8>)`: 解密后的明文
/// - `Err(String)`: 解密失败原因
pub fn deobfuscate(data: &[u8], key: &[u8; 64]) -> Result<Vec<u8>, String> {
    if data.is_empty() {
        return Err("数据为空".into());
    }

    let mut out = vec![0u8; data.len()];
    let ret = unsafe {
        asm_deobfuscate(data.as_ptr(), data.len(), key.as_ptr(), out.as_mut_ptr())
    };

    if ret != 0 {
        Err("ASM 解密失败".into())
    } else {
        Ok(out)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    /// 测试 ASM 加密/解密往返正确性
    #[test]
    fn test_obfuscate_deobfuscate_roundtrip() {
        let data = b"Hello, furry/\xE4\xBA\x8C\xE6\xAC\xA1\xE5\x85\x83 community! ASM \
            \xE7\xA7\x81\xE6\x9C\x89\xE5\x8A\xA0\xE5\xAF\x86\xE6\xB5\x8B\xE8\xAF\x95\xE3\x80\x82";
        let key = b"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";

        let encrypted = obfuscate(data, key).expect("加密失败");
        assert_eq!(encrypted.len(), data.len(), "密文长度应与明文相同");
        assert_ne!(encrypted, data, "密文不应与明文相同");

        let decrypted = deobfuscate(&encrypted, key).expect("解密失败");
        assert_eq!(decrypted, data, "解密结果应与原始明文一致");
    }

    /// 测试空数据返回错误
    #[test]
    fn test_empty_data() {
        let key = b"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
        assert!(obfuscate(&[], key).is_err());
        assert!(deobfuscate(&[], key).is_err());
    }

    /// 测试不同密钥产生不同密文
    #[test]
    fn test_different_keys() {
        let data = b"Test message";
        let key1 = b"0000000000000000000000000000000000000000000000000000000000000000";
        let key2 = b"1111111111111111111111111111111111111111111111111111111111111111";

        let enc1 = obfuscate(data, key1).expect("加密失败");
        let enc2 = obfuscate(data, key2).expect("加密失败");
        assert_ne!(enc1, enc2, "不同密钥应产生不同密文");
    }
}
