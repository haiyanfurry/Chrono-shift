/**
 * Chrono-shift AI 聊天会话实现
 * C++17
 */
#include "AIChatSession.h"

#include <sstream>
#include <algorithm>

namespace chrono {
namespace client {
namespace ai {

AIChatSession::AIChatSession(
    std::unique_ptr<AIProvider> provider,
    int max_history)
    : provider_(std::move(provider))
    , max_history_(max_history)
    , system_prompt_("你是一个有用的 AI 助手。") {
}

std::string AIChatSession::send_message(
    const std::string& content,
    std::function<void(const std::string&)> callback) {

    // 添加用户消息到历史
    history_.push_back({"user", content});

    // 构建完整上下文 (含系统提示词)
    std::vector<ChatMessage> context;
    context.push_back({"system", system_prompt_});
    context.insert(context.end(), history_.begin(), history_.end());

    // 调用 Provider
    std::string reply;
    if (callback) {
        reply = provider_->chat(context, [&](const std::string& chunk) {
            callback(chunk);
        });
    } else {
        reply = provider_->chat(context);
    }

    // 添加回复到历史
    history_.push_back({"assistant", reply});

    // 修剪历史
    trim_history();

    return reply;
}

void AIChatSession::add_message(const std::string& role, const std::string& content) {
    history_.push_back({role, content});
    trim_history();
}

std::string AIChatSession::get_context_text() const {
    std::ostringstream oss;
    for (const auto& msg : history_) {
        oss << "[" << msg.role << "] " << msg.content << "\n";
    }
    return oss.str();
}

void AIChatSession::clear_history() {
    history_.clear();
}

void AIChatSession::set_system_prompt(const std::string& prompt) {
    system_prompt_ = prompt;
}

std::string AIChatSession::regenerate(
    std::function<void(const std::string&)> callback) {

    // 找到最后一条用户消息
    auto it = std::find_if(history_.rbegin(), history_.rend(),
        [](const ChatMessage& m) { return m.role == "user"; });

    if (it == history_.rend()) {
        return "";
    }

    // 移除最后一条 AI 回复 (如果有)
    if (!history_.empty() && history_.back().role == "assistant") {
        history_.pop_back();
    }

    // 重新发送
    std::vector<ChatMessage> context;
    context.push_back({"system", system_prompt_});
    context.insert(context.end(), history_.begin(), history_.end());

    std::string reply;
    if (callback) {
        reply = provider_->chat(context, callback);
    } else {
        reply = provider_->chat(context);
    }

    history_.push_back({"assistant", reply});
    trim_history();

    return reply;
}

void AIChatSession::trim_history() {
    // 保留系统提示词和最近的 max_history_ 条消息
    if (static_cast<int>(history_.size()) > max_history_) {
        history_.erase(history_.begin(),
                       history_.begin() + (history_.size() - max_history_));
    }
}

} // namespace ai
} // namespace client
} // namespace chrono
