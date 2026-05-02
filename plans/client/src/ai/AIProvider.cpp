/**
 * Chrono-shift AI Provider 实现
 * C++17
 */
#include "AIProvider.h"

#include <memory>

namespace chrono {
namespace client {
namespace ai {

// OpenAIProvider 的前向声明 - 实际实现在 OpenAIProvider.cpp
std::unique_ptr<AIProvider> CreateOpenAIProvider(const AIConfig& config);
std::unique_ptr<AIProvider> CreateCustomProvider(const AIConfig& config);
std::unique_ptr<AIProvider> CreateGeminiProvider(const AIConfig& config);

std::unique_ptr<AIProvider> AIProvider::create(
    AIProviderType type,
    const AIConfig& config) {
    switch (type) {
        case AIProviderType::kOpenAI:
        case AIProviderType::kDeepSeek:
        case AIProviderType::kXAI:
        case AIProviderType::kOllama:
            // OpenAI, DeepSeek, xAI Grok, Ollama 均使用 OpenAI 兼容 API
            return CreateOpenAIProvider(config);
        case AIProviderType::kGemini:
            // Google Gemini 使用专有 API
            return CreateGeminiProvider(config);
        case AIProviderType::kCustom:
            return CreateCustomProvider(config);
        default:
            return nullptr;
    }
}

ProviderInfo AIProvider::get_provider_info(AIProviderType type) {
    auto preset = AIConfig::get_preset(type);
    return ProviderInfo{
        preset.name,
        preset.default_endpoint,
        preset.default_model,
        preset.requires_api_key
    };
}

} // namespace ai
} // namespace client
} // namespace chrono
