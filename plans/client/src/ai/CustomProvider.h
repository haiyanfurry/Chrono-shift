/**
 * Chrono-shift 自定义 AI Provider
 * C++17
 *
 * 支持用户自定义 API 的 AI Provider 实现
 */
#ifndef CHRONO_CLIENT_AI_CUSTOM_PROVIDER_H
#define CHRONO_CLIENT_AI_CUSTOM_PROVIDER_H

#include "AIProvider.h"

namespace chrono {
namespace client {
namespace ai {

/**
 * 自定义 API Provider
 *
 * 用户可配置任意 API 端点和请求格式
 */
class CustomProvider : public AIProvider {
public:
    explicit CustomProvider(const AIConfig& config);

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

    AIProviderType type() const override { return AIProviderType::kCustom; }

    std::string name() const override { return "自定义 API"; }

private:
    /** 存储的配置副本 */
    AIConfig config_;

    /**
     * 发送 HTTP 请求到自定义端点
     */
    std::string http_post(const std::string& endpoint,
                          const std::string& body,
                          const std::string& auth_header);
};

} // namespace ai
} // namespace client
} // namespace chrono

#endif // CHRONO_CLIENT_AI_CUSTOM_PROVIDER_H
