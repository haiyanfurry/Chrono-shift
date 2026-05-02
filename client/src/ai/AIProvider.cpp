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

std::unique_ptr<AIProvider> AIProvider::create(
    AIProviderType type,
    const AIConfig& config) {
    switch (type) {
        case AIProviderType::kOpenAI:
            return CreateOpenAIProvider(config);
        case AIProviderType::kCustom:
            return CreateCustomProvider(config);
        default:
            return nullptr;
    }
}

} // namespace ai
} // namespace client
} // namespace chrono
