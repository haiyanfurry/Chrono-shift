/**
 * Chrono-shift C++ Rust FFI 桥接实现
 */
#include "RustBridge.h"
#include "../util/Logger.h"

namespace chrono {
namespace ffi {

static bool s_initialized = false;

// ============================================================
// RustString
// ============================================================
RustString::RustString(char* ptr)
    : ptr_(ptr)
{
}

RustString::~RustString()
{
    if (ptr_) {
        rust_free_string(ptr_);
        ptr_ = nullptr;
    }
}

RustString::RustString(RustString&& other) noexcept
    : ptr_(other.ptr_)
{
    other.ptr_ = nullptr;
}

RustString& RustString::operator=(RustString&& other) noexcept
{
    if (this != &other) {
        if (ptr_) {
            rust_free_string(ptr_);
        }
        ptr_ = other.ptr_;
        other.ptr_ = nullptr;
    }
    return *this;
}

// ============================================================
// RustBridge
// ============================================================
bool RustBridge::init(const std::string& config_path)
{
    if (s_initialized) {
        return true;
    }
    int ret = rust_server_init(config_path.c_str());
    if (ret == 0) {
        s_initialized = true;
        LOG_INFO("[FFI] Rust security module initialized");
        return true;
    } else {
        LOG_ERROR("[FFI] Failed to initialize Rust security module (ret=%d)", ret);
        return false;
    }
}

std::string RustBridge::hash_password(const std::string& password)
{
    RustString result(rust_hash_password(password.c_str()));
    if (result.is_null()) {
        LOG_ERROR("[FFI] rust_hash_password failed");
        return "";
    }
    return result.str();
}

bool RustBridge::verify_password(const std::string& password, const std::string& hash)
{
    int ret = rust_verify_password(password.c_str(), hash.c_str());
    return ret == 0;
}

std::string RustBridge::generate_jwt(const std::string& user_id)
{
    RustString result(rust_generate_jwt(user_id.c_str()));
    if (result.is_null()) {
        LOG_ERROR("[FFI] rust_generate_jwt failed");
        return "";
    }
    return result.str();
}

std::string RustBridge::verify_jwt(const std::string& token)
{
    RustString result(rust_verify_jwt(token.c_str()));
    if (result.is_null()) {
        LOG_ERROR("[FFI] rust_verify_jwt failed");
        return "";
    }
    return result.str();
}

std::string RustBridge::version()
{
    RustString result(rust_version());
    return result.str();
}

bool RustBridge::is_initialized()
{
    return s_initialized;
}

} // namespace ffi
} // namespace chrono
