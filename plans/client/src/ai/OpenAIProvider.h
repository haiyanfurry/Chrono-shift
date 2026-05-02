/**
 * Chrono-shift OpenAI 兼容 API Provider
 * C++17
 *
 * 实现 OpenAI 兼容的 HTTP API 调用
 * 支持: OpenAI, DeepSeek, xAI Grok, Ollama (本地)
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
 * 支持所有 OpenAI 兼容协议的服务:
 * - OpenAI (api.openai.com)
 * - DeepSeek (api.deepseek.com)
 * - xAI Grok (api.x.ai)
 * - Ollama (localhost:11434)
 * - 以及任何兼容 OpenAI API 的自定义服务
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

    /** 返回实际的提供商类型 (动态: kOpenAI/kDeepSeek/kXAI/kOllama) */
    AIProviderType type() const override { return config_.provider_type; }

    /** 返回实际的提供商名称 */
    std::string name() const override {
        return AIConfig::provider_name(config_.provider_type);
    }

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
     * 发送 HTTP 请求 (通过 WinHTTP)
     * @param body JSON 请求体
     * @return HTTP 响应体
     */
    std::string http_post(const std::string& body);
};

} // namespace ai
} // namespace client
} // namespace chrono

#endif // CHRONO_CLIENT_AI_OPENAI_PROVIDER_H
