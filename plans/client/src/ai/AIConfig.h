/**
 * Chrono-shift AI 配置结构
 * C++17
 *
 * 定义 AI 提供商的配置参数，支持 6 种提供商的多选模式
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
 *
 * 枚举值对应 JSON 存储的数字，修改需保持向后兼容
 */
enum class AIProviderType : std::uint8_t {
    kNone      = 0,   // 未配置
    kOpenAI    = 1,   // OpenAI
    kDeepSeek  = 2,   // DeepSeek
    kXAI       = 3,   // xAI Grok
    kOllama    = 4,   // Ollama 本地
    kGemini    = 5,   // Google Gemini
    kCustom    = 6    // 自定义 API
};

/**
 * 提供商预设信息
 */
struct ProviderPreset {
    const char* name;              // 显示名称
    const char* default_endpoint;  // 默认 API 端点
    const char* default_model;     // 默认模型
    bool        requires_api_key;  // 是否需要 API Key
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

    /** 模型名称 (如 gpt-4o, deepseek-v4-flash) */
    std::string model_name = "gpt-4o";

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
        if (provider_type == AIProviderType::kNone) return false;
        if (api_endpoint.empty()) return false;
        // Ollama 本地模型不需要 API key
        if (provider_type == AIProviderType::kOllama) return true;
        return !api_key.empty();
    }

    /**
     * 判断是否为 OpenAI 兼容协议
     * OpenAI / DeepSeek / xAI / Ollama 均使用 /v1/chat/completions
     */
    static bool is_openai_compatible(AIProviderType type) {
        return type == AIProviderType::kOpenAI ||
               type == AIProviderType::kDeepSeek ||
               type == AIProviderType::kXAI ||
               type == AIProviderType::kOllama;
    }

    /**
     * 获取提供商显示名称
     */
    static const char* provider_name(AIProviderType type) {
        switch (type) {
            case AIProviderType::kOpenAI:   return "OpenAI";
            case AIProviderType::kDeepSeek: return "DeepSeek";
            case AIProviderType::kXAI:      return "xAI Grok";
            case AIProviderType::kOllama:   return "Ollama";
            case AIProviderType::kGemini:   return "Google Gemini";
            case AIProviderType::kCustom:   return "自定义 API";
            default:                        return "未配置";
        }
    }

    /**
     * 获取提供商预设信息
     */
    static ProviderPreset get_preset(AIProviderType type) {
        switch (type) {
            case AIProviderType::kOpenAI:
                return {"OpenAI", "https://api.openai.com", "gpt-4o", true};
            case AIProviderType::kDeepSeek:
                return {"DeepSeek", "https://api.deepseek.com", "deepseek-v4-flash", true};
            case AIProviderType::kXAI:
                return {"xAI Grok", "https://api.x.ai", "grok-3", true};
            case AIProviderType::kOllama:
                return {"Ollama", "http://localhost:11434", "llama3", false};
            case AIProviderType::kGemini:
                return {"Google Gemini", "https://generativelanguage.googleapis.com", "gemini-2.0-flash", true};
            case AIProviderType::kCustom:
                return {"自定义 API", "", "", false};
            default:
                return {"未配置", "", "", false};
        }
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
