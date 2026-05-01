/**
 * Chrono-shift 会话管理
 * C++17 重构版
 */
#include "SessionManager.h"
#include "../util/Logger.h"

namespace chrono {
namespace client {
namespace storage {

SessionManager::SessionManager() = default;
SessionManager::~SessionManager() = default;

int SessionManager::init()
{
    LOG_INFO("会话管理器初始化");
    return 0;
}

int SessionManager::save(const std::string& user_id, const std::string& username,
                         const std::string& token)
{
    state_.user_id   = user_id;
    state_.username  = username;
    state_.token     = token;
    state_.logged_in = true;

    LOG_INFO("会话已保存: user=%s, token=%.16s...",
             user_id.c_str(), token.c_str());
    return 0;
}

std::string SessionManager::get_token() const
{
    return state_.logged_in ? state_.token : std::string();
}

bool SessionManager::is_logged_in() const
{
    return state_.logged_in;
}

std::string SessionManager::get_user_id() const
{
    return state_.user_id;
}

std::string SessionManager::get_username() const
{
    return state_.username;
}

void SessionManager::clear()
{
    state_ = SessionState{};
    LOG_DEBUG("会话已清除");
}

SessionState SessionManager::get_state() const
{
    return state_;
}

} // namespace storage
} // namespace client
} // namespace chrono
