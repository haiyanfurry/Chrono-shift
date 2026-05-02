/**
 * chrono_client_security.h
 * Chrono-shift 客户端安全模块 Rust FFI C 头文件
 *
 * 由 cargo build 自动生成，也可手动维护。
 * 该头文件声明 Rust crate `chrono_client_security` 导出的所有 extern "C" 函数。
 */
#ifndef CHRONO_CLIENT_SECURITY_H
#define CHRONO_CLIENT_SECURITY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* ─────────── 初始化与资源管理 ─────────── */

/**
 * 初始化客户端安全模块
 * @param app_data_path 应用数据目录路径
 * @return 0 成功, -1 失败
 */
int rust_client_init(const char* app_data_path);

/**
 * 释放 Rust 分配的 C 字符串
 * @param s 由 Rust 分配的字符串指针
 */
void rust_client_free_string(char* s);

/**
 * 获取客户端安全模块版本
 * @return 版本字符串 (需调用 rust_client_free_string 释放)
 */
char* rust_client_version(void);

/* ─────────── E2E 加密 (AES-256-GCM) ─────────── */

/**
 * 生成 E2E 密钥对
 * @return Base64 编码的公钥 (需调用 rust_client_free_string 释放)
 */
char* rust_client_generate_keypair(void);

/**
 * 使用对方公钥加密消息
 * @param plaintext   明文
 * @param pubkey_b64  对方公钥 (Base64 编码)
 * @return Base64 编码的密文 (含 nonce)，失败返回 NULL
 */
char* rust_client_encrypt_e2e(const char* plaintext, const char* pubkey_b64);

/**
 * 使用己方私钥解密消息
 * @param ciphertext_b64 密文 (Base64 编码，含 nonce)
 * @param privkey_b64    己方私钥 (Base64 编码)
 * @return 明文，失败返回 NULL
 */
char* rust_client_decrypt_e2e(const char* ciphertext_b64, const char* privkey_b64);

/* ─────────── ASM 私有混淆加密 (512 位密钥) ─────────── */

/**
 * 使用 ASM 私有混淆加密数据
 * @param data_base64 输入数据的 Base64 编码
 * @param key_hex     512 位密钥十六进制字符串 (128 hex 字符)
 * @return Base64 编码的混淆后数据，失败返回 NULL
 */
char* rust_client_obfuscate_message(const char* data_base64, const char* key_hex);

/**
 * 使用 ASM 私有混淆解密数据
 * @param data_base64 混淆后数据的 Base64 编码
 * @param key_hex     512 位密钥十六进制字符串 (128 hex 字符)
 * @return Base64 编码的原始数据，失败返回 NULL
 */
char* rust_client_deobfuscate_message(const char* data_base64, const char* key_hex);

/* ─────────── 会话管理 ─────────── */

/**
 * 保存会话数据
 * @param key   键
 * @param value 值
 * @return 0 成功, -1 失败
 */
int rust_session_save(const char* key, const char* value);

/**
 * 获取当前登录 token
 * @return token 字符串 (需调用 rust_client_free_string 释放)
 */
char* rust_session_get_token(void);

/**
 * 检查是否已登录
 * @return 1 已登录, 0 未登录
 */
int rust_session_is_logged_in(void);

/**
 * 清除会话
 */
void rust_session_clear(void);

#ifdef __cplusplus
}
#endif

#endif /* CHRONO_CLIENT_SECURITY_H */
