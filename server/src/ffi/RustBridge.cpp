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

int RustBridge::validate_string(const std::string& input)
{
    if (input.empty()) {
        return 0; // 空字符串安全
    }
    // 调用 Rust FFI: rust_validate_utf8_safe
    return rust_validate_utf8_safe(
        reinterpret_cast<const unsigned char*>(input.data()),
        input.size()
    );
}

std::string RustBridge::validation_result_description(int result)
{
    RustString desc(rust_validation_result_description(result));
    return desc.str();
}

bool RustBridge::is_initialized()
{
    return s_initialized;
}

// ============================================================
// RateLimiter
// ============================================================

bool RustBridge::rate_limiter_init(unsigned long long window_secs, size_t max_requests)
{
    return rust_rate_limiter_init(window_secs, max_requests) == 0;
}

bool RustBridge::rate_limiter_allow(const std::string& key)
{
    return rust_rate_limiter_allow(key.c_str()) == 1;
}

void RustBridge::rate_limiter_cleanup()
{
    rust_rate_limiter_cleanup();
}

void RustBridge::rate_limiter_reset(const std::string& key)
{
    rust_rate_limiter_reset(key.c_str());
}

// ============================================================
// InputSanitizer
// ============================================================

std::string RustBridge::sanitize_username(const std::string& input)
{
    RustString result(rust_sanitize_username(input.c_str()));
    return result.is_null() ? "" : result.str();
}

std::string RustBridge::sanitize_display_name(const std::string& input)
{
    RustString result(rust_sanitize_display_name(input.c_str()));
    return result.is_null() ? "" : result.str();
}

std::string RustBridge::sanitize_message(const std::string& input)
{
    RustString result(rust_sanitize_message(input.c_str()));
    return result.is_null() ? "" : result.str();
}

int RustBridge::check_password_strength(const std::string& password)
{
    return rust_check_password_strength(password.c_str());
}

bool RustBridge::is_valid_email(const std::string& email)
{
    return rust_is_valid_email(email.c_str()) == 1;
}

std::string RustBridge::escape_html(const std::string& input)
{
    RustString result(rust_escape_html(input.c_str()));
    return result.is_null() ? "" : result.str();
}

// ============================================================
// SessionManager
// ============================================================

bool RustBridge::session_init(unsigned long long timeout_secs)
{
    return rust_session_init(timeout_secs) == 0;
}

std::string RustBridge::session_create(const std::string& user_id, const std::string& metadata)
{
    RustString result(rust_session_create(user_id.c_str(), metadata.c_str()));
    return result.is_null() ? "" : result.str();
}

std::string RustBridge::session_validate(const std::string& token)
{
    RustString result(rust_session_validate(token.c_str()));
    return result.is_null() ? "" : result.str();
}

void RustBridge::session_remove(const std::string& token)
{
    rust_session_remove(token.c_str());
}

void RustBridge::session_cleanup()
{
    rust_session_cleanup();
}

bool RustBridge::session_refresh(const std::string& token)
{
    return rust_session_refresh(token.c_str()) == 1;
}

int RustBridge::session_active_count()
{
    return rust_session_active_count();
}

void RustBridge::session_remove_user(const std::string& user_id)
{
    rust_session_remove_user(user_id.c_str());
}

// ============================================================
// CSRF
// ============================================================

std::string RustBridge::csrf_generate_token()
{
    RustString result(rust_csrf_generate_token());
    return result.is_null() ? "" : result.str();
}

bool RustBridge::csrf_validate_token(const std::string& token, const std::string& stored_token)
{
    return rust_csrf_validate_token(token.c_str(), stored_token.c_str()) == 1;
}

// ============================================================
// QQ OAuth Client
// ============================================================

bool RustBridge::qq_init(const std::string& app_id, const std::string& app_key, const std::string& redirect_uri)
{
    return rust_qq_init(app_id.c_str(), app_key.c_str(), redirect_uri.c_str()) == 0;
}

std::string RustBridge::qq_build_auth_url(const std::string& state)
{
    RustString result(rust_qq_build_auth_url(state.c_str()));
    return result.is_null() ? "" : result.str();
}

std::string RustBridge::qq_exchange_code(const std::string& code)
{
    RustString result(rust_qq_exchange_code(code.c_str()));
    return result.is_null() ? "" : result.str();
}

std::string RustBridge::qq_get_user_info(const std::string& access_token, const std::string& open_id)
{
    RustString result(rust_qq_get_user_info(access_token.c_str(), open_id.c_str()));
    return result.is_null() ? "" : result.str();
}

bool RustBridge::qq_verify_token(const std::string& access_token, const std::string& open_id)
{
    return rust_qq_verify_token(access_token.c_str(), open_id.c_str()) == 1;
}

// ============================================================
// WeChat OAuth Client
// ============================================================

bool RustBridge::wechat_init(const std::string& app_id, const std::string& app_key, const std::string& redirect_uri)
{
    return rust_wechat_init(app_id.c_str(), app_key.c_str(), redirect_uri.c_str()) == 0;
}

std::string RustBridge::wechat_build_auth_url(const std::string& state)
{
    RustString result(rust_wechat_build_auth_url(state.c_str()));
    return result.is_null() ? "" : result.str();
}

std::string RustBridge::wechat_exchange_code(const std::string& code)
{
    RustString result(rust_wechat_exchange_code(code.c_str()));
    return result.is_null() ? "" : result.str();
}

std::string RustBridge::wechat_get_user_info(const std::string& access_token, const std::string& open_id)
{
    RustString result(rust_wechat_get_user_info(access_token.c_str(), open_id.c_str()));
    return result.is_null() ? "" : result.str();
}

bool RustBridge::wechat_verify_token(const std::string& access_token, const std::string& open_id)
{
    return rust_wechat_verify_token(access_token.c_str(), open_id.c_str()) == 1;
}

// ============================================================
// Email Verifier (SMTP)
// ============================================================

bool RustBridge::email_init(const std::string& host, int port, const std::string& username, const std::string& password, const std::string& from_addr, const std::string& from_name, bool use_tls)
{
    return rust_email_init(host.c_str(), port, username.c_str(), password.c_str(), from_addr.c_str(), from_name.c_str(), use_tls ? 1 : 0) == 0;
}

bool RustBridge::email_send_code(const std::string& to_email, const std::string& code)
{
    return rust_email_send_code(to_email.c_str(), code.c_str()) == 0;
}

void RustBridge::email_cleanup()
{
    rust_email_cleanup();
}

} // namespace ffi
} // namespace chrono
