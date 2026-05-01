/**
 * Chrono-shift C++ 用户处理器
 * C++17 重构版
 */
#ifndef CHRONO_CPP_USER_HANDLER_H
#define CHRONO_CPP_USER_HANDLER_H

#include "../json/JsonValue.h"
#include "../db/Database.h"
#include <string>

namespace chrono {
namespace handler {

/**
 * 用户业务逻辑处理器
 */
class UserHandler {
public:
    explicit UserHandler(db::Database& database);

    /**
     * 用户注册
     */
    json::JsonValue handle_register(const json::JsonValue& params);

    /**
     * 用户登录 (用户名/密码)
     */
    json::JsonValue handle_login(const json::JsonValue& params);

    /**
     * 获取用户信息
     */
    json::JsonValue handle_get_profile(const json::JsonValue& params);

    /**
     * 更新个人资料
     */
    json::JsonValue handle_update_profile(const json::JsonValue& params);

    /**
     * 搜索用户
     */
    json::JsonValue handle_search(const json::JsonValue& params);

private:
    db::Database& db_;
};

} // namespace handler
} // namespace chrono

#endif // CHRONO_CPP_USER_HANDLER_H
