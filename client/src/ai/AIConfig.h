/**
 * Chrono-shift AI 配置结构
 * C++17
 *
 * 定义 AI 提供商的配置参数
 */
#ifndef CHRONO_CLIENT_AI_CONFIG_H
#define CHRONO_CLIENT_AI_CONFIG_H

#include <cstdint>
#include <string>

namespace chrono {
namespace client {
namespace ai {

/**
 * AI 提供商类型
 */
enum class AIProviderType : std::uint8_t {
    kNone    = 0,
    kOpenAI  = 1,   // OpenAI 兼容 API
    kCustom  = 2    // 自定义 API
};

/**
 * AI 配置结构
 */
struct AIConfig {
    /** 提供商类型 */
    AIProviderType provider_type = AIProviderType::kNone;

    /** API 端点 URL */
    std::string api_endpoint;

    /** API 密钥 */
    std::string api_key;

    /** 模型名称 (如 gpt-3.5-turbo, gpt-4) */
    std::string model_name = "gpt-3.5-turbo";

    /** 最大生成 Token 数 */
    int max_tokens = 2048;

    /** 温度参数 (0.0 - 2.0) */
    float temperature = 0.7f;

    /** 是否启用智能回复 */
    bool enable_smart_reply = false;

    /** 是否启用消息摘要 */
    bool enable_summarize = false;

    /** 是否启用翻译助手 */
    bool enable_translate = false;

    /** 系统提示词 */
    std::string system_prompt = "你是一个有用的 AI 助手。";

    /** 配置是否有效 */
    bool is_valid() const {
        return provider_type != AIProviderType::kNone &&
               !api_endpoint.empty() &&
               !api_key.empty();
    }

    /** 转为 JSON 字符串 (用于存储) */
    std::string to_json() const;

    /** 从 JSON 字符串解析 */
    static AIConfig from_json(const std::string& json);
};

} // namespace ai
} // namespace client
} // namespace chrono

#endif // CHRONO_CLIENT_AI_CONFIG_H
