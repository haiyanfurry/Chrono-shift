/**
 * Chrono-shift 客户端加密引擎
 * C++17 重构版
 *
 * 尝试通过 Rust FFI 调用底层加密实现，
 * 编译时检测到 RUST_FEATURE_ENABLED 时链接 Rust 库。
 * 如果未定义该宏，则提供纯 C++ 占位实现。
 */
#include "CryptoEngine.h"
#include "../util/Logger.h"

#ifdef RUST_FEATURE_ENABLED
#include "../../client/security/include/chrono_client_security.h"
#endif

#include <cstdlib>
#include <cstring>
#include <vector>

namespace chrono {
namespace client {
namespace security {

CryptoEngine::CryptoEngine() = default;
CryptoEngine::~CryptoEngine() = default;

int CryptoEngine::init()
{
#ifdef RUST_FEATURE_ENABLED
    /* 通过 Rust FFI 初始化 */
    int ret = rust_client_init(".");
    if (ret != 0) {
        LOG_ERROR("Rust 安全模块初始化失败");
        return -1;
    }
    LOG_INFO("加密引擎初始化完成 (Rust 后端)");
#else
    LOG_INFO("加密引擎初始化完成 (纯 C++ 占位模式)");
#endif
    initialized_ = true;
    return 0;
}

int CryptoEngine::generate_keypair(std::string& pubkey_b64)
{
    if (!initialized_) return -1;

#ifdef RUST_FEATURE_ENABLED
    char* result = rust_client_generate_keypair();
    if (!result) {
        LOG_ERROR("生成密钥对失败");
        return -1;
    }
    pubkey_b64 = result;
    rust_client_free_string(result);
    return 0;
#else
    /* 纯 C++ 占位：返回一个模拟公钥 */
    pubkey_b64 = "MOCK_PUBKEY_FOR_TESTING";
    LOG_DEBUG("生成密钥对 (占位模式)");
    return 0;
#endif
}

int CryptoEngine::encrypt_e2e(const std::string& plaintext,
                              const std::string& pubkey_b64,
                              std::string& ciphertext)
{
    if (!initialized_) return -1;

#ifdef RUST_FEATURE_ENABLED
    char* result = rust_client_encrypt_e2e(plaintext.c_str(), pubkey_b64.c_str());
    if (!result) {
        LOG_ERROR("E2E 加密失败");
        return -1;
    }
    ciphertext = result;
    rust_client_free_string(result);
    return 0;
#else
    /* 纯 C++ 占位：Base64-like 编码 */
    (void)pubkey_b64;
    LOG_DEBUG("E2E 加密 (占位模式)");
    ciphertext = "MOCK_ENCRYPTED:" + plaintext;
    return 0;
#endif
}

int CryptoEngine::decrypt_e2e(const std::string& ciphertext_b64,
                              const std::string& privkey_b64,
                              std::string& plaintext)
{
    if (!initialized_) return -1;

#ifdef RUST_FEATURE_ENABLED
    char* result = rust_client_decrypt_e2e(ciphertext_b64.c_str(), privkey_b64.c_str());
    if (!result) {
        LOG_ERROR("E2E 解密失败");
        return -1;
    }
    plaintext = result;
    rust_client_free_string(result);
    return 0;
#else
    /* 纯 C++ 占位 */
    (void)privkey_b64;
    LOG_DEBUG("E2E 解密 (占位模式)");
    static const std::string prefix = "MOCK_ENCRYPTED:";
    if (ciphertext_b64.compare(0, prefix.size(), prefix) == 0) {
        plaintext = ciphertext_b64.substr(prefix.size());
        return 0;
    }
    return -1;
#endif
}

} // namespace security
} // namespace client
} // namespace chrono
