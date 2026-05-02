/**
 * Chrono-shift Google Gemini Provider
 * C++17
 *
 * 实现 Google Gemini API 的专用 Provider
 * Gemini 使用独特的 API 格式，与 OpenAI 不兼容
 */
#ifndef CHRONO_CLIENT_AI_GEMINI_PROVIDER_H
#define CHRONO_CLIENT_AI_GEMINI_PROVIDER_H

#include "AIProvider.h"

namespace chrono {
namespace client {
namespace ai {

/**
 * Google Gemini API Provider
 *
 * Google Gemini 使用专有 API:
 * - 端点: https://generativelanguage.googleapis.com/v1beta/models/{model}:generateContent
 * - 鉴权: API key 作为 URL 参数 (?key=...)
 * - 请求: contents[{parts:[{text:...}]}]
 * - 响应: candidates[0].content.parts[0].text
 */
class GeminiProvider : public AIProvider {
public:
    explicit GeminiProvider(const AIConfig& config);

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

    AIProviderType type() const override { return AIProviderType::kGemini; }

    std::string name() const override { return "Google Gemini"; }

private:
    /** 存储的配置副本 */
    AIConfig config_;

    /**
     * 构建 Gemini 格式的请求体
     * @param messages 消息列表 (role: system/user/assistant)
     * @return JSON 请求体 (Gemini 格式)
     */
    std::string build_gemini_request(const std::vector<ChatMessage>& messages);

    /**
     * 解析 Gemini API 响应
     * @param response_body HTTP 响应体
     * @return 提取的回复文本
     */
    std::string parse_gemini_response(const std::string& response_body);

    /**
     * 发送 HTTP 请求 (通过 WinHTTP)
     * API key 作为 URL 参数传递
     * @param url 完整 URL (含 ?key=...)
     * @param body JSON 请求体
     * @return HTTP 响应体
     */
    std::string http_post(const std::string& url, const std::string& body);
};

} // namespace ai
} // namespace client
} // namespace chrono

#endif // CHRONO_CLIENT_AI_GEMINI_PROVIDER_H
