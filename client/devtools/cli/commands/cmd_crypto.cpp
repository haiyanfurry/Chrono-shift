/**
 * cmd_crypto.cpp — 加密测试命令 (AES-256-GCM)
 * 对应 debug_cli.c:2464 cmd_crypto
 *
 * C++23 转换: std::println, std::string_view, std::chrono
 *
 * 注意: 需要链接 OpenSSL (libcrypto)
 */
#include "../devtools_cli.hpp"

#if HTTPS_SUPPORT
#include <openssl/evp.h>
#endif

#include <chrono>    // std::chrono::system_clock
#include <print>     // std::println
#include <string_view> // std::string_view
#include <cstring>   // std::strlen, std::strcmp
#include <cstdlib>   // std::srand, std::rand

/* ============================================================
 * crypto 命令 - E2E 加密测试
 * ============================================================ */
#if HTTPS_SUPPORT
static int cmd_crypto(int argc, char** argv)
{
    if (argc < 1) {
        std::println("用法:");
        std::println("  crypto test                    - 测试 AES-256-GCM 加密/解密");
        return -1;
    }

    const std::string_view subcmd = argv[0];

    if (subcmd == "test") {
        std::println("");
        std::println("  ╔══════════════════════════════════════════════════════════╗");
        std::println("  ║     E2E 加密测试 (AES-256-GCM)                          ║");
        std::println("  ╚══════════════════════════════════════════════════════════╝");
        std::println("");

        /* 使用 OpenSSL EVP 进行 AES-256-GCM 加密/解密测试 */
        unsigned char key[32];
        unsigned char nonce[12];
        const char* plaintext = "Hello, Chrono-shift! 这是一个 E2E 加密测试消息。";
        unsigned char ciphertext[256];
        unsigned char decrypted[256];
        int ciphertext_len = 0;
        int decrypted_len = 0;

        /* 生成随机密钥 */
        std::println("  [1/5] 生成 AES-256 密钥...");
        {
            std::srand(static_cast<unsigned int>(
                std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())));
            for (int i = 0; i < 32; i++) key[i] = static_cast<unsigned char>(std::rand() % 256);
        }
        std::print("        密钥: ");
        for (int i = 0; i < 32; i++) std::print("{:02X}", key[i]);
        std::println("");
        std::println("");

        /* 生成随机 nonce */
        std::println("  [2/5] 生成随机 Nonce (12 字节)...");
        {
            for (int i = 0; i < 12; i++) nonce[i] = static_cast<unsigned char>(std::rand() % 256);
        }
        std::print("        Nonce: ");
        for (int i = 0; i < 12; i++) std::print("{:02X}", nonce[i]);
        std::println("");
        std::println("");

        /* 加密 */
        std::println("  [3/5] 加密明文 ({} 字节)...", std::strlen(plaintext));
        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
        if (!ctx) {
            std::println(stderr, "[-] EVP_CIPHER_CTX_new 失败");
            return -1;
        }
        if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
            std::println(stderr, "[-] EVP_EncryptInit_ex 失败");
            EVP_CIPHER_CTX_free(ctx);
            return -1;
        }
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 12, nullptr) != 1) {
            std::println(stderr, "[-] 设置 IV 长度失败");
            EVP_CIPHER_CTX_free(ctx);
            return -1;
        }
        if (EVP_EncryptInit_ex(ctx, nullptr, nullptr, key, nonce) != 1) {
            std::println(stderr, "[-] EVP_EncryptInit_ex (key, nonce) 失败");
            EVP_CIPHER_CTX_free(ctx);
            return -1;
        }
        int outlen = 0;
        if (EVP_EncryptUpdate(ctx, ciphertext, &outlen,
                              reinterpret_cast<const unsigned char*>(plaintext),
                              static_cast<int>(std::strlen(plaintext))) != 1) {
            std::println(stderr, "[-] EVP_EncryptUpdate 失败");
            EVP_CIPHER_CTX_free(ctx);
            return -1;
        }
        ciphertext_len = outlen;
        if (EVP_EncryptFinal_ex(ctx, ciphertext + ciphertext_len, &outlen) != 1) {
            std::println(stderr, "[-] EVP_EncryptFinal_ex 失败");
            EVP_CIPHER_CTX_free(ctx);
            return -1;
        }
        ciphertext_len += outlen;

        unsigned char tag[16];
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag) != 1) {
            std::println(stderr, "[-] 获取 GCM 标签失败");
            EVP_CIPHER_CTX_free(ctx);
            return -1;
        }
        EVP_CIPHER_CTX_free(ctx);

        std::print("        密文: ");
        for (int i = 0; i < ciphertext_len; i++) std::print("{:02X}", ciphertext[i]);
        std::println("");
        std::print("        标签: ");
        for (int i = 0; i < 16; i++) std::print("{:02X}", tag[i]);
        std::println("");
        std::println("");

        /* 解密 */
        std::println("  [4/5] 解密密文 ({} 字节)...", ciphertext_len);
        ctx = EVP_CIPHER_CTX_new();
        if (!ctx) {
            std::println(stderr, "[-] EVP_CIPHER_CTX_new (解密) 失败");
            return -1;
        }
        if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
            std::println(stderr, "[-] EVP_DecryptInit_ex (解密) 失败");
            EVP_CIPHER_CTX_free(ctx);
            return -1;
        }
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 12, nullptr) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            return -1;
        }
        if (EVP_DecryptInit_ex(ctx, nullptr, nullptr, key, nonce) != 1) {
            std::println(stderr, "[-] EVP_DecryptInit_ex (解密 key, nonce) 失败");
            EVP_CIPHER_CTX_free(ctx);
            return -1;
        }
        if (EVP_DecryptUpdate(ctx, decrypted, &outlen, ciphertext, ciphertext_len) != 1) {
            std::println(stderr, "[-] EVP_DecryptUpdate 失败");
            EVP_CIPHER_CTX_free(ctx);
            return -1;
        }
        decrypted_len = outlen;

        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, tag) != 1) {
            std::println(stderr, "[-] 设置 GCM 标签验证失败");
            EVP_CIPHER_CTX_free(ctx);
            return -1;
        }
        int ret = EVP_DecryptFinal_ex(ctx, decrypted + decrypted_len, &outlen);
        EVP_CIPHER_CTX_free(ctx);

        decrypted[decrypted_len] = 0;

        std::println("        解密结果: {}", reinterpret_cast<const char*>(decrypted));
        std::println("");

        /* 验证 */
        std::println("  [5/5] 验证...");
        if (ret > 0 && std::strcmp(reinterpret_cast<const char*>(decrypted), plaintext) == 0) {
            std::println("        ✓ AES-256-GCM 加密/解密测试通过!");
            std::println("        ✓ 数据完整性验证通过 (GCM 认证标签)");
        } else {
            std::println("        ✗ 验证失败!");
            return -1;
        }
        std::println("");

        std::println("  说明:");
        std::println("  实现: client/security/src/crypto.rs (Rust FFI)");
        std::println("  或直接使用 OpenSSL EVP 库 (本测试)");
        return 0;

    } else {
        std::println(stderr, "未知 crypto 子命令: {}", subcmd);
        return -1;
    }
}

#else   /* !HTTPS_SUPPORT */
static int cmd_crypto(int /*argc*/, char** /*argv*/)
{
    std::println(stderr, "[-] crypto 命令需要 OpenSSL 支持 (HTTPS_SUPPORT=0)");
    return -1;
}
#endif  /* HTTPS_SUPPORT */

extern "C" int init_cmd_crypto(void)
{
    register_command("crypto",
        "E2E 加密测试 (AES-256-GCM)",
        "crypto test",
        cmd_crypto);
    return 0;
}
