/**
 * Chrono-shift C++ 安全模块实现
 */
#include "SecurityManager.h"
#include "../util/StringUtils.h"
#include "../util/Logger.h"

#include <random>
#include <regex>
#include <cctype>
#include <sstream>

namespace chrono {
namespace security {

// ============================================================
// RateLimiter
// ============================================================
RateLimiter::RateLimiter(size_t max_requests, size_t window_ms)
    : max_requests_(max_requests)
    , window_ms_(window_ms)
{
}

bool RateLimiter::allow(const std::string& key)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto now = std::chrono::steady_clock::now();

    auto it = entries_.find(key);
    if (it == entries_.end()) {
        // 新条目
        Entry entry;
        entry.count = 1;
        entry.window_start = now;
        entries_[key] = entry;
        return true;
    }

    auto& entry = it->second;
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - entry.window_start).count();

    if (elapsed >= static_cast<long long>(window_ms_)) {
        // 窗口已过期，重置
        entry.count = 1;
        entry.window_start = now;
        return true;
    }

    if (entry.count >= max_requests_) {
        return false; // 超过限制
    }

    entry.count++;
    return true;
}

void RateLimiter::cleanup()
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto now = std::chrono::steady_clock::now();

    for (auto it = entries_.begin(); it != entries_.end(); ) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - it->second.window_start).count();
        if (elapsed >= static_cast<long long>(window_ms_)) {
            it = entries_.erase(it);
        } else {
            ++it;
        }
    }
}

// ============================================================
// InputSanitizer
// ============================================================
std::string InputSanitizer::sanitize_username(const std::string& input)
{
    std::string result;
    result.reserve(input.size());
    for (char c : input) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-') {
            result += c;
        }
    }
    // 限制长度
    if (result.size() > 32) {
        result = result.substr(0, 32);
    }
    return result;
}

std::string InputSanitizer::sanitize_display_name(const std::string& input)
{
    std::string result;
    result.reserve(input.size());
    for (char c : input) {
        if (std::isprint(static_cast<unsigned char>(c)) && c != '<' && c != '>' &&
            c != '&' && c != '"' && c != '\'') {
            result += c;
        }
    }
    if (result.size() > 64) {
        result = result.substr(0, 64);
    }
    return result;
}

std::string InputSanitizer::sanitize_message(const std::string& input)
{
    // 转义 HTML 特殊字符
    return escape_html(input);
}

std::string InputSanitizer::check_password_strength(const std::string& password)
{
    if (password.size() < 8) {
        return "密码长度至少 8 个字符";
    }
    if (password.size() > 128) {
        return "密码长度不能超过 128 个字符";
    }

    bool has_upper = false;
    bool has_lower = false;
    bool has_digit = false;
    bool has_special = false;

    for (char c : password) {
        if (std::isupper(static_cast<unsigned char>(c))) has_upper = true;
        else if (std::islower(static_cast<unsigned char>(c))) has_lower = true;
        else if (std::isdigit(static_cast<unsigned char>(c))) has_digit = true;
        else has_special = true;
    }

    if (!has_upper) return "密码需要包含大写字母";
    if (!has_lower) return "密码需要包含小写字母";
    if (!has_digit) return "密码需要包含数字";
    if (!has_special) return "密码需要包含特殊字符";

    return ""; // 通过
}

bool InputSanitizer::is_valid_email(const std::string& email)
{
    // 简单邮箱格式验证
    static const std::regex pattern(
        R"(^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$)");
    return std::regex_match(email, pattern);
}

std::string InputSanitizer::escape_html(const std::string& input)
{
    std::string result;
    result.reserve(input.size());
    for (char c : input) {
        switch (c) {
        case '&':  result += "&amp;";  break;
        case '<':  result += "&lt;";   break;
        case '>':  result += "&gt;";   break;
        case '"':  result += "&quot;"; break;
        case '\'': result += "&apos;";  break;
        default:   result += c;        break;
        }
    }
    return result;
}

// ============================================================
// SessionManager
// ============================================================
std::string SessionManager::create_session(const std::string& user_id,
                                            uint64_t duration_ms)
{
    std::lock_guard<std::mutex> lock(mutex_);

    // 生成随机令牌
    std::string token = util::StringUtils::generate_uuid();

    Session session;
    session.user_id = user_id;
    session.expires_at = std::chrono::steady_clock::now() +
        std::chrono::milliseconds(duration_ms);

    sessions_[token] = std::move(session);
    return token;
}

std::string SessionManager::validate_session(const std::string& token)
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = sessions_.find(token);
    if (it == sessions_.end()) {
        return "";
    }

    auto now = std::chrono::steady_clock::now();
    if (now >= it->second.expires_at) {
        sessions_.erase(it);
        return "";
    }

    return it->second.user_id;
}

void SessionManager::destroy_session(const std::string& token)
{
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_.erase(token);
}

void SessionManager::cleanup()
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto now = std::chrono::steady_clock::now();

    for (auto it = sessions_.begin(); it != sessions_.end(); ) {
        if (now >= it->second.expires_at) {
            it = sessions_.erase(it);
        } else {
            ++it;
        }
    }
}

// ============================================================
// CsrfProtector
// ============================================================
std::string CsrfProtector::generate_token()
{
    return util::StringUtils::generate_uuid();
}

bool CsrfProtector::validate_token(const std::string& token, const std::string& expected)
{
    return token == expected && !token.empty();
}

} // namespace security
} // namespace chrono
