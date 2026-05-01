/**
 * Chrono-shift C++ 消息处理器实现
 */
#include "MessageHandler.h"
#include "../util/Logger.h"
#include "../util/StringUtils.h"

namespace chrono {
namespace handler {

using namespace chrono::json;

MessageHandler::MessageHandler(db::Database& database)
    : db_(database)
{
}

JsonValue MessageHandler::handle_send(const JsonValue& params)
{
    std::string from_id = params["from_id"].get_string("");
    std::string to_id   = params["to_id"].get_string("");
    std::string content = params["content"].get_string("");
    int msg_type        = static_cast<int>(params["msg_type"].get_double(0.0));

    if (from_id.empty() || to_id.empty()) {
        return build_error("发送者和接收者 ID 不能为空");
    }
    if (content.empty()) {
        return build_error("消息内容不能为空");
    }

    db::MessageData msg;
    msg.message_id = util::StringUtils::generate_uuid();
    msg.from_id    = from_id;
    msg.to_id      = to_id;
    msg.content    = content;
    msg.timestamp  = chrono::util::StringUtils::timestamp_ms();
    msg.msg_type   = msg_type;

    if (!db_.save_message(msg)) {
        return build_error("保存消息失败");
    }

    JsonValue data = json_object();
    data.object_insert("message_id", JsonValue(msg.message_id));
    data.object_insert("timestamp",  JsonValue(static_cast<double>(msg.timestamp)));

    return build_success(data);
}

JsonValue MessageHandler::handle_get_history(const JsonValue& params)
{
    std::string user_id  = params["user_id"].get_string("");
    std::string other_id = params["other_id"].get_string("");
    int limit  = static_cast<int>(params["limit"].get_double(50.0));
    int offset = static_cast<int>(params["offset"].get_double(0.0));

    if (user_id.empty() || other_id.empty()) {
        return build_error("用户 ID 不能为空");
    }

    auto messages = db_.get_messages(user_id, other_id, limit, offset);

    JsonValue arr = json_array();
    for (const auto& msg : messages) {
        JsonValue item = json_object();
        item.object_insert("message_id", JsonValue(msg.message_id));
        item.object_insert("from_id",    JsonValue(msg.from_id));
        item.object_insert("to_id",      JsonValue(msg.to_id));
        item.object_insert("content",    JsonValue(msg.content));
        item.object_insert("timestamp",  JsonValue(static_cast<double>(msg.timestamp)));
        item.object_insert("msg_type",   JsonValue(static_cast<double>(msg.msg_type)));
        arr.array_push_back(std::move(item));
    }

    return build_success(arr);
}

JsonValue MessageHandler::handle_delete(const JsonValue& params)
{
    std::string message_id = params["message_id"].get_string("");
    if (message_id.empty()) {
        return build_error("消息 ID 不能为空");
    }

    // 消息删除：将文件内容置空标记
    // 简单版本直接删除文件
    auto msg = db_.get_message(message_id);
    if (!msg) {
        return build_error("消息不存在");
    }

    // 为简化，标记删除 = 重新保存空内容
    msg->content = "[deleted]";
    if (!db_.save_message(*msg)) {
        return build_error("删除失败");
    }

    return build_success(JsonValue(json_object()));
}

} // namespace handler
} // namespace chrono
