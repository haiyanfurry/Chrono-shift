/**
 * Chrono-shift AI 聊天会话管理
 * C++17
 *
 * 管理 AI 聊天上下文，维护消息历史
 */
#ifndef CHRONO_CLIENT_AI_CHAT_SESSION_H
#define CHRONO_CLIENT_AI_CHAT_SESSION_H

#include <string>
#include <vector>
#include <memory>

#include "AIProvider.h"

namespace chrono {
namespace client {
namespace ai {

/**
 * AI 聊天会话
 *
 * 维护对话上下文，支持多轮对话
 */
class AIChatSession {
public:
    /**
     * 构造函数
     * @param provider AI Provider 实例
     * @param max_history 最大历史消息数
     */
    explicit AIChatSession(
        std::unique_ptr<AIProvider> provider,
        int max_history = 50);

    ~AIChatSession() = default;

    // 禁止拷贝
    AIChatSession(const AIChatSession&) = delete;
    AIChatSession& operator=(const AIChatSession&) = delete;

    /** 允许移动 */
    AIChatSession(AIChatSession&&) = default;
    AIChatSession& operator=(AIChatSession&&) = default;

    /**
     * 添加用户消息并获取 AI 回复
     * @param content 用户消息内容
     * @param callback 流式回调 (可选)
     * @return AI 回复内容
     */
    std::string send_message(
        const std::string& content,
        std::function<void(const std::string&)> callback = nullptr);

    /**
     * 添加消息到历史
     * @param role   角色 (user/assistant/system)
     * @param content 内容
     */
    void add_message(const std::string& role, const std::string& content);

    /**
     * 获取完整上下文 (用于显示)
     */
    const std::vector<ChatMessage>& get_history() const {
        return history_;
    }

    /**
     * 获取上下文文本 (格式化后)
     */
    std::string get_context_text() const;

    /**
     * 清空历史
     */
    void clear_history();

    /**
     * 设置系统提示词
     * @param prompt 系统提示词
     */
    void set_system_prompt(const std::string& prompt);

    /**
     * 获取系统提示词
     */
    const std::string& get_system_prompt() const {
        return system_prompt_;
    }

    /**
     * 重新生成上一条回复
     * @param callback 流式回调 (可选)
     * @return 新的回复内容
     */
    std::string regenerate(
        std::function<void(const std::string&)> callback = nullptr);

    /**
     * 获取底层 Provider
     */
    AIProvider& provider() { return *provider_; }

    /**
     * 获取底层 Provider (const)
     */
    const AIProvider& provider() const { return *provider_; }

    /**
     * 设置最大历史消息数
     */
    void set_max_history(int max) { max_history_ = max; }

private:
    /** AI Provider */
    std::unique_ptr<AIProvider> provider_;

    /** 聊天历史 */
    std::vector<ChatMessage> history_;

    /** 系统提示词 */
    std::string system_prompt_;

    /** 最大历史消息数 */
    int max_history_;

    /** 修剪历史到最大数量 */
    void trim_history();
};

} // namespace ai
} // namespace client
} // namespace chrono

#endif // CHRONO_CLIENT_AI_CHAT_SESSION_H
