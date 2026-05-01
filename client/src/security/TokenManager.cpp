/**
 * Chrono-shift 令牌管理器
 * C++17 重构版
 */
#include "TokenManager.h"
#include "../util/Logger.h"

namespace chrono {
namespace client {
namespace security {

TokenManager::TokenManager() = default;
TokenManager::~TokenManager() = default;

int TokenManager::init()
{
    LOG_INFO("令牌管理器初始化");
    return 0;
}

void TokenManager::set_access_token(const std::string& token, uint32_t expires_in)
{
    access_token_ = token;
    if (expires_in > 0) {
        auto now = std::chrono::system_clock::now();
        auto dur = std::chrono::duration_cast<std::chrono::seconds>(
            now.time_since_epoch()).count();
        expires_at_ = static_cast<uint64_t>(dur) + expires_in;
    } else {
        expires_at_ = 0; /* 不过期 */
    }
    LOG_DEBUG("访问令牌已设置 (expires_in=%u)", expires_in);
}

void TokenManager::set_refresh_token(const std::string& token)
{
    refresh_token_ = token;
    LOG_DEBUG("刷新令牌已设置");
}

std::string TokenManager::get_access_token() const
{
    return access_token_;
}

std::string TokenManager::get_refresh_token() const
{
    return refresh_token_;
}

bool TokenManager::is_token_valid() const
{
    if (access_token_.empty()) return false;
    if (expires_at_ == 0) return true; /* 不过期 */

    auto now = std::chrono::system_clock::now();
    auto dur = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()).count();

    /* 留 60 秒余量，提前过期 */
    constexpr uint64_t kGracePeriod = 60;
    return static_cast<uint64_t>(dur) + kGracePeriod < expires_at_;
}

void TokenManager::clear()
{
    access_token_.clear();
    refresh_token_.clear();
    expires_at_ = 0;
    LOG_DEBUG("令牌已清除");
}

} // namespace security
} // namespace client
} // namespace chrono
