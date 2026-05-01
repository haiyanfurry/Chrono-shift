/**
 * Chrono-shift C++ 消息处理器
 * C++17 重构版
 */
#ifndef CHRONO_CPP_MESSAGE_HANDLER_H
#define CHRONO_CPP_MESSAGE_HANDLER_H

#include "../json/JsonValue.h"
#include "../db/Database.h"
#include <string>

namespace chrono {
namespace handler {

class MessageHandler {
public:
    explicit MessageHandler(db::Database& database);

    json::JsonValue handle_send(const json::JsonValue& params);
    json::JsonValue handle_get_history(const json::JsonValue& params);
    json::JsonValue handle_delete(const json::JsonValue& params);

private:
    db::Database& db_;
};

} // namespace handler
} // namespace chrono

#endif // CHRONO_CPP_MESSAGE_HANDLER_H
