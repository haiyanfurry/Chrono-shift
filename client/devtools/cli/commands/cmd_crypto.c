/**
 * cmd_crypto.c — 加密测试命令 (AES-256-GCM)
 * 对应 debug_cli.c:2464 cmd_crypto
 *
 * 注意: 需要链接 OpenSSL (libcrypto)
 */
#include "../devtools_cli.h"
#include <openssl/evp.h>
#include <time.h>

/* ============================================================
 * crypto 命令 - E2E 加密测试
 * ============================================================ */
static int cmd_crypto(int argc, char** argv)
{
    if (argc < 1) {
        printf("用法:\n");
        printf("  crypto test                    - 测试 AES-256-GCM 加密/解密\n");
        return -1;
    }

    const char* subcmd = argv[0];

    if (strcmp(subcmd, "test") == 0) {
        printf("\n");
        printf("  ╔══════════════════════════════════════════════════════════╗\n");
        printf("  ║     E2E 加密测试 (AES-256-GCM)                          ║\n");
        printf("  ╚══════════════════════════════════════════════════════════╝\n");
        printf("\n");

        /* 使用 OpenSSL EVP 进行 AES-256-GCM 加密/解密测试 */
        unsigned char key[32];
        unsigned char nonce[12];
        const char* plaintext = "Hello, Chrono-shift! 这是一个 E2E 加密测试消息。";
        unsigned char ciphertext[256];
        unsigned char decrypted[256];
        int ciphertext_len = 0;
        int decrypted_len = 0;

        /* 生成随机密钥 */
        printf("  [1/5] 生成 AES-256 密钥...\n");
        {
            srand((unsigned int)time(NULL));
            for (int i = 0; i < 32; i++) key[i] = (unsigned char)(rand() % 256);
        }
        printf("        密钥: ");
        for (int i = 0; i < 32; i++) printf("%02X", key[i]);
        printf("\n\n");

        /* 生成随机 nonce */
        printf("  [2/5] 生成随机 Nonce (12 字节)...\n");
        {
            for (int i = 0; i < 12; i++) nonce[i] = (unsigned char)(rand() % 256);
        }
        printf("        Nonce: ");
        for (int i = 0; i < 12; i++) printf("%02X", nonce[i]);
        printf("\n\n");

        /* 加密 */
        printf("  [3/5] 加密明文 (%zu 字节)...\n", strlen(plaintext));
        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
        if (!ctx) {
            fprintf(stderr, "[-] EVP_CIPHER_CTX_new 失败\n");
            return -1;
        }
        if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1) {
            fprintf(stderr, "[-] EVP_EncryptInit_ex 失败\n");
            EVP_CIPHER_CTX_free(ctx);
            return -1;
        }
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 12, NULL) != 1) {
            fprintf(stderr, "[-] 设置 IV 长度失败\n");
            EVP_CIPHER_CTX_free(ctx);
            return -1;
        }
        if (EVP_EncryptInit_ex(ctx, NULL, NULL, key, nonce) != 1) {
            fprintf(stderr, "[-] EVP_EncryptInit_ex (key, nonce) 失败\n");
            EVP_CIPHER_CTX_free(ctx);
            return -1;
        }
        int outlen = 0;
        if (EVP_EncryptUpdate(ctx, ciphertext, &outlen,
                              (const unsigned char*)plaintext, (int)strlen(plaintext)) != 1) {
            fprintf(stderr, "[-] EVP_EncryptUpdate 失败\n");
            EVP_CIPHER_CTX_free(ctx);
            return -1;
        }
        ciphertext_len = outlen;
        if (EVP_EncryptFinal_ex(ctx, ciphertext + ciphertext_len, &outlen) != 1) {
            fprintf(stderr, "[-] EVP_EncryptFinal_ex 失败\n");
            EVP_CIPHER_CTX_free(ctx);
            return -1;
        }
        ciphertext_len += outlen;

        unsigned char tag[16];
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag) != 1) {
            fprintf(stderr, "[-] 获取 GCM 标签失败\n");
            EVP_CIPHER_CTX_free(ctx);
            return -1;
        }
        EVP_CIPHER_CTX_free(ctx);

        printf("        密文: ");
        for (int i = 0; i < ciphertext_len; i++) printf("%02X", ciphertext[i]);
        printf("\n");
        printf("        标签: ");
        for (int i = 0; i < 16; i++) printf("%02X", tag[i]);
        printf("\n\n");

        /* 解密 */
        printf("  [4/5] 解密密文 (%d 字节)...\n", ciphertext_len);
        ctx = EVP_CIPHER_CTX_new();
        if (!ctx) {
            fprintf(stderr, "[-] EVP_CIPHER_CTX_new (解密) 失败\n");
            return -1;
        }
        if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1) {
            fprintf(stderr, "[-] EVP_DecryptInit_ex (解密) 失败\n");
            EVP_CIPHER_CTX_free(ctx);
            return -1;
        }
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 12, NULL) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            return -1;
        }
        if (EVP_DecryptInit_ex(ctx, NULL, NULL, key, nonce) != 1) {
            fprintf(stderr, "[-] EVP_DecryptInit_ex (解密 key, nonce) 失败\n");
            EVP_CIPHER_CTX_free(ctx);
            return -1;
        }
        if (EVP_DecryptUpdate(ctx, decrypted, &outlen, ciphertext, ciphertext_len) != 1) {
            fprintf(stderr, "[-] EVP_DecryptUpdate 失败\n");
            EVP_CIPHER_CTX_free(ctx);
            return -1;
        }
        decrypted_len = outlen;

        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, tag) != 1) {
            fprintf(stderr, "[-] 设置 GCM 标签验证失败\n");
            EVP_CIPHER_CTX_free(ctx);
            return -1;
        }
        int ret = EVP_DecryptFinal_ex(ctx, decrypted + decrypted_len, &outlen);
        EVP_CIPHER_CTX_free(ctx);

        decrypted[decrypted_len] = 0;

        printf("        解密结果: %s\n", decrypted);
        printf("\n");

        /* 验证 */
        printf("  [5/5] 验证...\n");
        if (ret > 0 && strcmp((const char*)decrypted, plaintext) == 0) {
            printf("        ✓ AES-256-GCM 加密/解密测试通过!\n");
            printf("        ✓ 数据完整性验证通过 (GCM 认证标签)\n");
        } else {
            printf("        ✗ 验证失败!\n");
            return -1;
        }
        printf("\n");

        printf("  说明:\n");
        printf("  实现: client/security/src/crypto.rs (Rust FFI)\n");
        printf("  或直接使用 OpenSSL EVP 库 (本测试)\n");
        return 0;

    } else {
        fprintf(stderr, "未知 crypto 子命令: %s\n", subcmd);
        return -1;
    }
}

int init_cmd_crypto(void)
{
    register_command("crypto",
        "E2E 加密测试 (AES-256-GCM)",
        "crypto test",
        cmd_crypto);
    return 0;
}
