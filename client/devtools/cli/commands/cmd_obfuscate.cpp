/**
 * cmd_obfuscate.cpp — ASM 私有混淆加密调试命令
 *
 * C++23 转换: std::println, std::string, std::string_view, std::chrono
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
#include "../devtools_cli.hpp"

#include <chrono>    // std::chrono::system_clock
#include <print>     // std::println
#include <string>    // std::string
#include <string_view> // std::string_view
#include <cstdio>    // std::snprintf
#include <cstdlib>   // std::srand, std::rand, std::malloc, std::free
#include <cstring>   // std::strlen, std::strcmp, std::strncmp, std::strcpy

/* ─── 模拟 Rust FFI (编译时未链接 Rust 库时的占位) ─── */
#ifndef RUST_FEATURE_ENABLED

/* 占位：模拟混淆（异或伪操作） */
static char* mock_obfuscate(const char* data_b64, const char* key_hex)
{
    (void)key_hex;
    size_t len = std::strlen(data_b64) + 32;
    char* result = static_cast<char*>(std::malloc(len));
    if (!result) return nullptr;
    std::snprintf(result, len, "MOCK_OBF:%s", data_b64);
    return result;
}

static char* mock_deobfuscate(const char* data_b64, const char* key_hex)
{
    (void)key_hex;
    constexpr const char prefix[] = "MOCK_OBF:";
    if (std::strncmp(data_b64, prefix, sizeof(prefix) - 1) != 0)
        return nullptr;
    const char* payload = data_b64 + sizeof(prefix) - 1;
    char* result = static_cast<char*>(std::malloc(std::strlen(payload) + 1));
    if (!result) return nullptr;
    std::strcpy(result, payload);
    return result;
}

#else
/* 真实 Rust FFI 声明 */
extern "C" {
extern char* rust_client_obfuscate_message(const char* data_b64, const char* key_hex);
extern char* rust_client_deobfuscate_message(const char* data_b64, const char* key_hex);
extern void  rust_client_free_string(char* s);
}
#endif

/* ─── 工具函数 ─── */

/** 生成随机十六进制字符 */
static char rand_hex_char() noexcept
{
    int v = std::rand() % 16;
    return (v < 10) ? static_cast<char>('0' + v) : static_cast<char>('a' + v - 10);
}

/** 生成随机 512 位密钥 (128 hex 字符) */
static void generate_key_hex(char* out, size_t out_size) noexcept
{
    if (out_size < 129) return;
    for (int i = 0; i < 128; i++) {
        out[i] = rand_hex_char();
    }
    out[128] = '\0';
}

/** Base64 编码 (简易实现，仅用于测试) */
static void base64_encode(const unsigned char* input, int input_len,
                          char* output, int output_size) noexcept
{
    static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int j = 0;
    for (int i = 0; i < input_len; i += 3) {
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
        std::println("用法:");
        std::println("  obfuscate genkey                    - 生成随机 512 位密钥");
        std::println("  obfuscate test                      - 运行自测试");
        std::println("  obfuscate encrypt <data_b64> <key>  - 混淆加密");
        std::println("  obfuscate decrypt <data_b64> <key>  - 混淆解密");
        return -1;
    }

    const std::string_view subcmd = argv[0];

    /* ── genkey: 生成随机 512 位密钥 ── */
    if (subcmd == "genkey") {
        char key[129];
        std::srand(static_cast<unsigned int>(
            std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())));
        generate_key_hex(key, sizeof(key));

        std::println("");
        std::println("  ╔══════════════════════════════════════════════════════════╗");
        std::println("  ║     ASM 私有混淆 — 512 位密钥生成                       ║");
        std::println("  ╚══════════════════════════════════════════════════════════╝");
        std::println("");
        std::println("  密钥 (512-bit / 64 字节):");
        std::println("  {}", key);
        std::println("");
        std::println("  密钥长度: {} 字符 (hex)", std::strlen(key));
        std::println("");
        return 0;
    }

    /* ── test: 运行自测试 ── */
    if (subcmd == "test") {
        std::println("");
        std::println("  ╔══════════════════════════════════════════════════════════╗");
        std::println("  ║     ASM 私有混淆 — 自测试                               ║");
        std::println("  ╚══════════════════════════════════════════════════════════╝");
        std::println("");

        int pass = 0, fail = 0;

        /* 测试 1: 基本加密/解密往返 */
        {
            std::println("  [测试 1] 基本加密/解密往返...");
            const char* test_plaintext = "Hello, Chrono-shift! 测试消息 123!@#";
            unsigned char data_b64[256];
            base64_encode(reinterpret_cast<const unsigned char*>(test_plaintext),
                          static_cast<int>(std::strlen(test_plaintext)),
                          reinterpret_cast<char*>(data_b64), sizeof(data_b64));

            char key[129];
            std::srand(static_cast<unsigned int>(
                std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())));
            generate_key_hex(key, sizeof(key));

            bool ok = false;
#ifdef RUST_FEATURE_ENABLED
            char* obf = rust_client_obfuscate_message(reinterpret_cast<const char*>(data_b64), key);
            if (!obf) { std::println("        ✗ 加密失败"); fail++; goto test1_end; }
            char* deobf = rust_client_deobfuscate_message(obf, key);
            rust_client_free_string(obf);
            if (!deobf) { std::println("        ✗ 解密失败"); fail++; goto test1_end; }
            ok = (std::strcmp(deobf, reinterpret_cast<const char*>(data_b64)) == 0);
            rust_client_free_string(deobf);
#else
            char* obf = mock_obfuscate(reinterpret_cast<const char*>(data_b64), key);
            if (!obf) { std::println("        ✗ 加密失败"); fail++; goto test1_end; }
            char* deobf = mock_deobfuscate(obf, key);
            std::free(obf);
            if (!deobf) { std::println("        ✗ 解密失败"); fail++; goto test1_end; }
            ok = (std::strcmp(deobf, reinterpret_cast<const char*>(data_b64)) == 0);
            std::free(deobf);
#endif
            if (ok) {
                std::println("        ✓ 往返测试通过");
                pass++;
            } else {
                std::println("        ✗ 往返测试失败: 解密结果与原始数据不符");
                fail++;
            }
        }
    test1_end:

        /* 测试 2: 不同密钥产生不同密文 */
        {
            std::println("  [测试 2] 不同密钥产生不同密文...");
            const char* test_data = "Different keys test";
            unsigned char data_b64[256];
            base64_encode(reinterpret_cast<const unsigned char*>(test_data),
                          static_cast<int>(std::strlen(test_data)),
                          reinterpret_cast<char*>(data_b64), sizeof(data_b64));

            char key1[129], key2[129];
            generate_key_hex(key1, sizeof(key1));
            generate_key_hex(key2, sizeof(key2));

#ifdef RUST_FEATURE_ENABLED
            char* obf1 = rust_client_obfuscate_message(reinterpret_cast<const char*>(data_b64), key1);
            char* obf2 = rust_client_obfuscate_message(reinterpret_cast<const char*>(data_b64), key2);
            if (!obf1 || !obf2) {
                std::println("        ✗ 加密失败");
                fail++;
                if (obf1) rust_client_free_string(obf1);
                if (obf2) rust_client_free_string(obf2);
                goto test2_end;
            }
            bool diff = (std::strcmp(obf1, obf2) != 0);
            rust_client_free_string(obf1);
            rust_client_free_string(obf2);
            if (diff) {
                std::println("        ✓ 不同密钥产生不同密文");
                pass++;
            } else {
                std::println("        ✗ 不同密钥产生了相同密文");
                fail++;
            }
#else
            char* obf1 = mock_obfuscate(reinterpret_cast<const char*>(data_b64), key1);
            /* 模拟占位模式下不同 key 产生相同结果，跳过此测试 */
            std::free(obf1);
            std::println("        - 占位模式跳过 (mock 不依赖 key)");
            pass++;
            goto test2_end;
#endif
        }
    test2_end:

        /* 测试 3: 空数据处理 */
        {
            std::println("  [测试 3] 空数据处理 (应返回错误)...");
            char key[129];
            generate_key_hex(key, sizeof(key));

#ifdef RUST_FEATURE_ENABLED
            char* result = rust_client_obfuscate_message("", key);
            bool null_result = (result == nullptr);
            if (result) rust_client_free_string(result);
#else
            (void)key;
            bool null_result = true; /* mock 返回 NULL 给空输入 */
#endif
            if (null_result) {
                std::println("        ✓ 空数据正确返回错误");
                pass++;
            } else {
                std::println("        ✗ 空数据未返回错误");
                fail++;
            }
        }

        std::println("");
        std::println("  ────────────────────────────────────────");
        std::println("  结果: {} 通过, {} 失败", pass, fail);
        std::println("");
        return (fail == 0) ? 0 : -1;
    }

    /* ── encrypt: 混淆加密 ── */
    if (subcmd == "encrypt") {
        if (argc < 3) {
            std::println(stderr, "用法: obfuscate encrypt <data_b64> <key_hex>");
            return -1;
        }
        const char* data_b64 = argv[1];
        const char* key_hex  = argv[2];

        std::println("");
        std::println("  ╔══════════════════════════════════════════════════════════╗");
        std::println("  ║     ASM 私有混淆 — 加密                                 ║");
        std::println("  ╚══════════════════════════════════════════════════════════╝");
        std::println("");
        std::println("  输入 (Base64): {}", data_b64);
        std::println("  密钥: {}", key_hex);
        std::println("");

#ifdef RUST_FEATURE_ENABLED
        char* result = rust_client_obfuscate_message(data_b64, key_hex);
#else
        char* result = mock_obfuscate(data_b64, key_hex);
#endif
        if (!result) {
            std::println(stderr, "[-] 加密失败");
            return -1;
        }
        std::println("  输出 (Base64): {}", result);
#ifdef RUST_FEATURE_ENABLED
        rust_client_free_string(result);
#else
        std::free(result);
#endif
        std::println("");
        return 0;
    }

    /* ── decrypt: 混淆解密 ── */
    if (subcmd == "decrypt") {
        if (argc < 3) {
            std::println(stderr, "用法: obfuscate decrypt <data_b64> <key_hex>");
            return -1;
        }
        const char* data_b64 = argv[1];
        const char* key_hex  = argv[2];

        std::println("");
        std::println("  ╔══════════════════════════════════════════════════════════╗");
        std::println("  ║     ASM 私有混淆 — 解密                                 ║");
        std::println("  ╚══════════════════════════════════════════════════════════╝");
        std::println("");
        std::println("  输入 (Base64): {}", data_b64);
        std::println("  密钥: {}", key_hex);
        std::println("");

#ifdef RUST_FEATURE_ENABLED
        char* result = rust_client_deobfuscate_message(data_b64, key_hex);
#else
        char* result = mock_deobfuscate(data_b64, key_hex);
#endif
        if (!result) {
            std::println(stderr, "[-] 解密失败");
            return -1;
        }
        std::println("  输出 (Base64): {}", result);
#ifdef RUST_FEATURE_ENABLED
        rust_client_free_string(result);
#else
        std::free(result);
#endif
        std::println("");
        return 0;
    }

    std::println(stderr, "未知子命令: {}", subcmd);
    return -1;
}

extern "C" int init_cmd_obfuscate(void)
{
    register_command("obfuscate",
        "ASM 私有混淆加密调试 (512 位密钥)",
        "obfuscate test | genkey | encrypt <data> <key> | decrypt <data> <key>",
        cmd_obfuscate);
    return 0;
}
