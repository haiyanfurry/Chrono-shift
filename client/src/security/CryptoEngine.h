/**
 * Chrono-shift 客户端加密引擎
 * C++17 重构版
 *
 * 提供端到端加密 (E2E) 功能。
 * 底层加密通过 Rust FFI (client/security/src/crypto.rs) 实现，
 * 如果 Rust 库不可用则提供纯 C++ 回退实现。
 */
#ifndef CHRONO_CLIENT_CRYPTO_ENGINE_H
#define CHRONO_CLIENT_CRYPTO_ENGINE_H

#include <string>
#include <cstdint>

namespace chrono {
namespace client {
namespace security {

/**
 * 加密引擎
 */
class CryptoEngine {
public:
    CryptoEngine();
    ~CryptoEngine();

    CryptoEngine(const CryptoEngine&) = delete;
    CryptoEngine& operator=(const CryptoEngine&) = delete;
    CryptoEngine(CryptoEngine&&) = default;
    CryptoEngine& operator=(CryptoEngine&&) = default;

    /**
     * 初始化加密引擎
     * @return 0 成功, -1 失败
     */
    int init();

    /**
     * 生成 E2E 密钥对
     * @param pubkey_b64[out] 公钥 (Base64 编码)
     * @return 0 成功, -1 失败
     */
    int generate_keypair(std::string& pubkey_b64);

    /**
     * 使用对方公钥加密消息 (E2E)
     * @param plaintext   明文
     * @param pubkey_b64  对方公钥 (Base64)
     * @param ciphertext[out] 密文 (Base64 编码，含 nonce)
     * @return 0 成功, -1 失败
     */
    int encrypt_e2e(const std::string& plaintext, const std::string& pubkey_b64,
                    std::string& ciphertext);

    /**
     * 使用己方私钥解密消息 (E2E)
     * @param ciphertext_b64 密文 (Base64 编码，含 nonce)
     * @param privkey_b64    己方私钥 (Base64)
     * @param plaintext[out] 明文
     * @return 0 成功, -1 失败
     */
    int decrypt_e2e(const std::string& ciphertext_b64, const std::string& privkey_b64,
                    std::string& plaintext);

    /**
     * 使用 ASM 私有混淆加密消息
     * @param data_b64  输入数据 (Base64 编码)
     * @param key_hex   512 位密钥十六进制字符串 (128 hex 字符)
     * @param obfuscated_b64[out] 混淆后数据 (Base64 编码)
     * @return 0 成功, -1 失败
     */
    int obfuscate_message(const std::string& data_b64, const std::string& key_hex,
                          std::string& obfuscated_b64);

    /**
     * 使用 ASM 私有混淆解密消息
     * @param data_b64  混淆后数据 (Base64 编码)
     * @param key_hex   512 位密钥十六进制字符串 (128 hex 字符)
     * @param plaintext_b64[out] 原始数据 (Base64 编码)
     * @return 0 成功, -1 失败
     */
    int deobfuscate_message(const std::string& data_b64, const std::string& key_hex,
                            std::string& plaintext_b64);

    /** 检查是否已初始化 */
    bool is_initialized() const { return initialized_; }

private:
    bool initialized_ = false;
};

} // namespace security
} // namespace client
} // namespace chrono

#endif // CHRONO_CLIENT_CRYPTO_ENGINE_H
