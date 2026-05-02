/**
 * Chrono-shift OpenAI 兼容 API Provider
 * C++17
 *
 * 实现 OpenAI 兼容的 HTTP API 调用
 */
#ifndef CHRONO_CLIENT_AI_OPENAI_PROVIDER_H
#define CHRONO_CLIENT_AI_OPENAI_PROVIDER_H

#include "AIProvider.h"

namespace chrono {
namespace client {
namespace ai {

/**
 * OpenAI 兼容 API Provider
 *
 * 支持 OpenAI、Azure OpenAI、以及任何兼容 OpenAI API 的服务
 */
class OpenAIProvider : public AIProvider {
public:
    explicit OpenAIProvider(const AIConfig& config);

    void set_config(const AIConfig& config) override;

    const AIConfig& get_config() const override { return config_; }

    std::string chat(
        const std::vector<ChatMessage>& messages,
        std::function<void(const std::string&)> callback = nullptr) override;

    std::string generate(
        const std::string& prompt,
        const std::string& params = "") override;

    bool is_available() const override;

    bool test_connection() override;

    AIProviderType type() const override { return AIProviderType::kOpenAI; }

    std::string name() const override { return "OpenAI 兼容"; }

private:
    /** 存储的配置副本 */
    AIConfig config_;

    /**
     * 构建 Chat Completion 请求体
     * @param messages 消息列表
     * @return JSON 请求体
     */
    std::string build_chat_request(const std::vector<ChatMessage>& messages);

    /**
     * 解析 Chat Completion 响应
     * @param response_body HTTP 响应体
     * @return 提取的回复文本
     */
    std::string parse_chat_response(const std::string& response_body);

    /**
     * 发送 HTTP 请求 (通过 WinHTTP 或 libcurl)
     * @param body JSON 请求体
     * @return HTTP 响应体
     */
    std::string http_post(const std::string& body);
};

} // namespace ai
} // namespace client
} // namespace chrono

#endif // CHRONO_CLIENT_AI_OPENAI_PROVIDER_H
