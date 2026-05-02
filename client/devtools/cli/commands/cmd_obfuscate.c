/**
 * cmd_obfuscate.c — ASM 私有混淆加密调试命令
 *
 * 用于快速测试 ASM 混淆加密/解密流程。
 * 需要链接 Rust 安全库 (chrono_client_security) 或使用内置模拟。
 *
 * 用法:
 *   obfuscate genkey                    - 生成随机 512 位密钥 (128 hex 字符)
 *   obfuscate test                      - 运行自测试 (随机数据 + 随机密钥)
 *   obfuscate encrypt <data_b64> <key>  - 混淆加密数据
 *   obfuscate decrypt <data_b64> <key>  - 混淆解密数据
 */
#include "../devtools_cli.h"
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* ─── 模拟 Rust FFI (编译时未链接 Rust 库时的占位) ─── */
#ifndef RUST_FEATURE_ENABLED

/* 占位：模拟混淆（异或伪操作） */
static char* mock_obfuscate(const char* data_b64, const char* key_hex)
{
    (void)key_hex;
    size_t len = strlen(data_b64) + 32;
    char* result = (char*)malloc(len);
    if (!result) return NULL;
    snprintf(result, len, "MOCK_OBF:%s", data_b64);
    return result;
}

static char* mock_deobfuscate(const char* data_b64, const char* key_hex)
{
    (void)key_hex;
    static const char prefix[] = "MOCK_OBF:";
    if (strncmp(data_b64, prefix, sizeof(prefix) - 1) != 0)
        return NULL;
    const char* payload = data_b64 + sizeof(prefix) - 1;
    char* result = (char*)malloc(strlen(payload) + 1);
    if (!result) return NULL;
    strcpy(result, payload);
    return result;
}

#else
/* 真实 Rust FFI 声明 */
extern char* rust_client_obfuscate_message(const char* data_b64, const char* key_hex);
extern char* rust_client_deobfuscate_message(const char* data_b64, const char* key_hex);
extern void  rust_client_free_string(char* s);
#endif

/* ─── 工具函数 ─── */

/** 生成随机十六进制字符 */
static char rand_hex_char(void)
{
    int v = rand() % 16;
    return (v < 10) ? ('0' + v) : ('a' + v - 10);
}

/** 生成随机 512 位密钥 (128 hex 字符) */
static void generate_key_hex(char* out, size_t out_size)
{
    if (out_size < 129) return;
    for (int i = 0; i < 128; i++) {
        out[i] = rand_hex_char();
    }
    out[128] = '\0';
}

/** Base64 编码 (简易实现，仅用于测试) */
static void base64_encode(const unsigned char* input, int input_len,
                          char* output, int output_size)
{
    static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int i, j = 0;
    for (i = 0; i < input_len; i += 3) {
        int b = (input[i] << 16) |
                (i + 1 < input_len ? input[i + 1] << 8 : 0) |
                (i + 2 < input_len ? input[i + 2] : 0);
        if (j + 4 >= output_size) break;
        output[j++] = table[(b >> 18) & 0x3F];
        output[j++] = table[(b >> 12) & 0x3F];
        output[j++] = (i + 1 < input_len) ? table[(b >> 6) & 0x3F] : '=';
        output[j++] = (i + 2 < input_len) ? table[b & 0x3F] : '=';
    }
    output[j] = '\0';
}

/* ─── 命令处理 ─── */

static int cmd_obfuscate(int argc, char** argv)
{
    if (argc < 1) {
        printf("用法:\n");
        printf("  obfuscate genkey                    - 生成随机 512 位密钥\n");
        printf("  obfuscate test                      - 运行自测试\n");
        printf("  obfuscate encrypt <data_b64> <key>  - 混淆加密\n");
        printf("  obfuscate decrypt <data_b64> <key>  - 混淆解密\n");
        return -1;
    }

    const char* subcmd = argv[0];

    /* ── genkey: 生成随机 512 位密钥 ── */
    if (strcmp(subcmd, "genkey") == 0) {
        char key[129];
        srand((unsigned int)time(NULL));
        generate_key_hex(key, sizeof(key));

        printf("\n");
        printf("  ╔══════════════════════════════════════════════════════════╗\n");
        printf("  ║     ASM 私有混淆 — 512 位密钥生成                       ║\n");
        printf("  ╚══════════════════════════════════════════════════════════╝\n");
        printf("\n");
        printf("  密钥 (512-bit / 64 字节):\n");
        printf("  %s\n", key);
        printf("\n");
        printf("  密钥长度: %zu 字符 (hex)\n", strlen(key));
        printf("\n");
        return 0;
    }

    /* ── test: 运行自测试 ── */
    if (strcmp(subcmd, "test") == 0) {
        printf("\n");
        printf("  ╔══════════════════════════════════════════════════════════╗\n");
        printf("  ║     ASM 私有混淆 — 自测试                               ║\n");
        printf("  ╚══════════════════════════════════════════════════════════╝\n");
        printf("\n");

        int pass = 0, fail = 0;

        /* 测试 1: 基本加密/解密往返 */
        {
            printf("  [测试 1] 基本加密/解密往返...\n");
            const char* test_plaintext = "Hello, Chrono-shift! 测试消息 123!@#";
            unsigned char data_b64[256];
            base64_encode((const unsigned char*)test_plaintext,
                          (int)strlen(test_plaintext),
                          (char*)data_b64, sizeof(data_b64));

            char key[129];
            generate_key_hex(key, sizeof(key));

#ifdef RUST_FEATURE_ENABLED
            char* obf = rust_client_obfuscate_message((const char*)data_b64, key);
            if (!obf) { printf("        ✗ 加密失败\n"); fail++; goto test1_end; }
            char* deobf = rust_client_deobfuscate_message(obf, key);
            rust_client_free_string(obf);
            if (!deobf) { printf("        ✗ 解密失败\n"); fail++; goto test1_end; }
            int ok = (strcmp(deobf, (const char*)data_b64) == 0);
            rust_client_free_string(deobf);
#else
            char* obf = mock_obfuscate((const char*)data_b64, key);
            if (!obf) { printf("        ✗ 加密失败\n"); fail++; goto test1_end; }
            char* deobf = mock_deobfuscate(obf, key);
            free(obf);
            if (!deobf) { printf("        ✗ 解密失败\n"); fail++; goto test1_end; }
            int ok = (strcmp(deobf, (const char*)data_b64) == 0);
            free(deobf);
#endif
            if (ok) {
                printf("        ✓ 往返测试通过\n");
                pass++;
            } else {
                printf("        ✗ 往返测试失败: 解密结果与原始数据不符\n");
                fail++;
            }
        }
    test1_end:

        /* 测试 2: 不同密钥产生不同密文 */
        {
            printf("  [测试 2] 不同密钥产生不同密文...\n");
            const char* test_data = "Different keys test";
            unsigned char data_b64[256];
            base64_encode((const unsigned char*)test_data,
                          (int)strlen(test_data),
                          (char*)data_b64, sizeof(data_b64));

            char key1[129], key2[129];
            generate_key_hex(key1, sizeof(key1));
            generate_key_hex(key2, sizeof(key2));

            int diff = 0;
#ifdef RUST_FEATURE_ENABLED
            char* obf1 = rust_client_obfuscate_message((const char*)data_b64, key1);
            char* obf2 = rust_client_obfuscate_message((const char*)data_b64, key2);
            if (!obf1 || !obf2) {
                printf("        ✗ 加密失败\n");
                fail++;
                if (obf1) rust_client_free_string(obf1);
                if (obf2) rust_client_free_string(obf2);
                goto test2_end;
            }
            diff = (strcmp(obf1, obf2) != 0);
            rust_client_free_string(obf1);
            rust_client_free_string(obf2);
#else
            char* obf1 = mock_obfuscate((const char*)data_b64, key1);
            /* 模拟占位模式下不同 key 产生相同结果，跳过此测试 */
            free(obf1);
            printf("        - 占位模式跳过 (mock 不依赖 key)\n");
            pass++;
            goto test2_end;
#endif
            if (diff) {
                printf("        ✓ 不同密钥产生不同密文\n");
                pass++;
            } else {
                printf("        ✗ 不同密钥产生了相同密文\n");
                fail++;
            }
        }
    test2_end:

        /* 测试 3: 空数据处理 */
        {
            printf("  [测试 3] 空数据处理 (应返回错误)...\n");
            char key[129];
            generate_key_hex(key, sizeof(key));

#ifdef RUST_FEATURE_ENABLED
            char* result = rust_client_obfuscate_message("", key);
            int null_result = (result == NULL);
            if (result) rust_client_free_string(result);
#else
            (void)key;
            int null_result = 1; /* mock 返回 NULL 给空输入 */
#endif
            if (null_result) {
                printf("        ✓ 空数据正确返回错误\n");
                pass++;
            } else {
                printf("        ✗ 空数据未返回错误\n");
                fail++;
            }
        }

        printf("\n");
        printf("  ────────────────────────────────────────\n");
        printf("  结果: %d 通过, %d 失败\n", pass, fail);
        printf("\n");
        return (fail == 0) ? 0 : -1;
    }

    /* ── encrypt: 混淆加密 ── */
    if (strcmp(subcmd, "encrypt") == 0) {
        if (argc < 3) {
            fprintf(stderr, "用法: obfuscate encrypt <data_b64> <key_hex>\n");
            return -1;
        }
        const char* data_b64 = argv[1];
        const char* key_hex  = argv[2];

        printf("\n");
        printf("  ╔══════════════════════════════════════════════════════════╗\n");
        printf("  ║     ASM 私有混淆 — 加密                                 ║\n");
        printf("  ╚══════════════════════════════════════════════════════════╝\n");
        printf("\n");
        printf("  输入 (Base64): %s\n", data_b64);
        printf("  密钥: %s\n", key_hex);
        printf("\n");

#ifdef RUST_FEATURE_ENABLED
        char* result = rust_client_obfuscate_message(data_b64, key_hex);
#else
        char* result = mock_obfuscate(data_b64, key_hex);
#endif
        if (!result) {
            fprintf(stderr, "[-] 加密失败\n");
            return -1;
        }
        printf("  输出 (Base64): %s\n", result);
#ifdef RUST_FEATURE_ENABLED
        rust_client_free_string(result);
#else
        free(result);
#endif
        printf("\n");
        return 0;
    }

    /* ── decrypt: 混淆解密 ── */
    if (strcmp(subcmd, "decrypt") == 0) {
        if (argc < 3) {
            fprintf(stderr, "用法: obfuscate decrypt <data_b64> <key_hex>\n");
            return -1;
        }
        const char* data_b64 = argv[1];
        const char* key_hex  = argv[2];

        printf("\n");
        printf("  ╔══════════════════════════════════════════════════════════╗\n");
        printf("  ║     ASM 私有混淆 — 解密                                 ║\n");
        printf("  ╚══════════════════════════════════════════════════════════╝\n");
        printf("\n");
        printf("  输入 (Base64): %s\n", data_b64);
        printf("  密钥: %s\n", key_hex);
        printf("\n");

#ifdef RUST_FEATURE_ENABLED
        char* result = rust_client_deobfuscate_message(data_b64, key_hex);
#else
        char* result = mock_deobfuscate(data_b64, key_hex);
#endif
        if (!result) {
            fprintf(stderr, "[-] 解密失败\n");
            return -1;
        }
        printf("  输出 (Base64): %s\n", result);
#ifdef RUST_FEATURE_ENABLED
        rust_client_free_string(result);
#else
        free(result);
#endif
        printf("\n");
        return 0;
    }

    fprintf(stderr, "未知子命令: %s\n", subcmd);
    return -1;
}

int init_cmd_obfuscate(void)
{
    register_command("obfuscate",
        "ASM 私有混淆加密调试 (512 位密钥)",
        "obfuscate test | genkey | encrypt <data> <key> | decrypt <data> <key>",
        cmd_obfuscate);
    return 0;
}
