/**
 * Chrono-shift AI Provider 抽象基类
 * C++17
 *
 * 定义统一的 AI 服务接口，支持 6 种 AI 提供商
 */
#ifndef CHRONO_CLIENT_AI_PROVIDER_H
#define CHRONO_CLIENT_AI_PROVIDER_H

#include <string>
#include <vector>
#include <functional>
#include <memory>

#include "AIConfig.h"

namespace chrono {
namespace client {
namespace ai {

/**
 * 聊天消息结构
 */
struct ChatMessage {
    std::string role;      // "system", "user", "assistant"
    std::string content;
};

/**
 * 提供商信息查询结果
 */
struct ProviderInfo {
    std::string display_name;       // 显示名称
    std::string default_endpoint;   // 默认 API 端点
    std::string default_model;      // 默认模型
    bool        requires_api_key;   // 是否需要 API Key
};

/**
 * AI Provider 抽象基类
 *
 * 所有 AI 提供商必须实现此接口
 */
class AIProvider {
public:
    virtual ~AIProvider() = default;

    /**
     * 设置配置
     * @param config AI 配置
     */
    virtual void set_config(const AIConfig& config) = 0;

    /**
     * 获取当前配置
     */
    virtual const AIConfig& get_config() const = 0;

    /**
     * 聊天补全
     * @param messages  消息历史
     * @param callback  流式回调 (返回完整文本时为 nullptr)
     * @return 生成的回复文本
     */
    virtual std::string chat(
        const std::vector<ChatMessage>& messages,
        std::function<void(const std::string&)> callback = nullptr) = 0;

    /**
     * 简单文本生成
     * @param prompt  提示词
     * @param params  额外参数 (JSON 格式)
     * @return 生成的文本
     */
    virtual std::string generate(
        const std::string& prompt,
        const std::string& params = "") = 0;

    /**
     * 检查提供商是否可用 (API 密钥已配置)
     */
    virtual bool is_available() const = 0;

    /**
     * 测试连接
     * @return true=连接成功
     */
    virtual bool test_connection() = 0;

    /**
     * 获取提供商类型
     */
    virtual AIProviderType type() const = 0;

    /**
     * 获取提供商名称
     */
    virtual std::string name() const = 0;

    /**
     * 创建 AI Provider 实例
     * @param type 提供商类型
     * @param config 配置
     * @return unique_ptr<AIProvider>
     */
    static std::unique_ptr<AIProvider> create(
        AIProviderType type,
        const AIConfig& config);

    /**
     * 获取提供商信息
     * @param type 提供商类型
     * @return ProviderInfo
     */
    static ProviderInfo get_provider_info(AIProviderType type);

protected:
    /** API 端点 */
    std::string api_endpoint_;

    /** API 密钥 */
    std::string api_key_;

    /** 模型名称 */
    std::string model_;

    /** 最大 Token */
    int max_tokens_ = 2048;

    /** 温度 */
    float temperature_ = 0.7f;
};

} // namespace ai
} // namespace client
} // namespace chrono

#endif // CHRONO_CLIENT_AI_PROVIDER_H
