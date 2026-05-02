/**
 * Chrono-shift AI 配置实现
 * C++17
 */
#include "AIConfig.h"

#include <sstream>

namespace chrono {
namespace client {
namespace ai {

std::string AIConfig::to_json() const {
    std::ostringstream oss;
    oss << "{"
        << "\"provider_type\":" << static_cast<int>(provider_type) << ","
        << "\"api_endpoint\":\"" << api_endpoint << "\","
        << "\"api_key\":\"" << api_key << "\","
        << "\"model_name\":\"" << model_name << "\","
        << "\"max_tokens\":" << max_tokens << ","
        << "\"temperature\":" << temperature << ","
        << "\"enable_smart_reply\":" << (enable_smart_reply ? "true" : "false") << ","
        << "\"enable_summarize\":" << (enable_summarize ? "true" : "false") << ","
        << "\"enable_translate\":" << (enable_translate ? "true" : "false") << ","
        << "\"system_prompt\":\"" << system_prompt << "\""
        << "}";
    return oss.str();
}

AIConfig AIConfig::from_json(const std::string& json) {
    AIConfig config;
    // 简化 JSON 解析 - 实际项目应使用完整 JSON 解析器
    auto find_value = [&json](const std::string& key) -> std::string {
        auto pos = json.find("\"" + key + "\"");
        if (pos == std::string::npos) return "";
        pos = json.find(':', pos + key.size() + 2);
        if (pos == std::string::npos) return "";
        pos++; // skip ':'
        while (pos < json.size() && json[pos] == ' ') pos++;
        if (pos >= json.size()) return "";
        if (json[pos] == '"') {
            pos++;
            auto end = json.find('"', pos);
            if (end == std::string::npos) return "";
            return json.substr(pos, end - pos);
        }
        auto end = json.find_first_of(",}", pos);
        if (end == std::string::npos) return "";
        return json.substr(pos, end - pos);
    };

    std::string pt = find_value("provider_type");
    if (!pt.empty()) {
        int pt_val = std::stoi(pt);
        // 支持值范围: 0 (kNone) ~ 6 (kCustom)
        if (pt_val >= 0 && pt_val <= 6) {
            config.provider_type = static_cast<AIProviderType>(pt_val);
        }
    }
    config.api_endpoint = find_value("api_endpoint");
    config.api_key = find_value("api_key");
    config.model_name = find_value("model_name");
    // 数值字段使用 find_value 后 stoi/stof
    auto mt = find_value("max_tokens");
    if (!mt.empty()) config.max_tokens = std::stoi(mt);
    auto tmp = find_value("temperature");
    if (!tmp.empty()) config.temperature = std::stof(tmp);

    return config;
}

} // namespace ai
} // namespace client
} // namespace chrono
