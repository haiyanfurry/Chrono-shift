/**
 * Chrono-shift 插件清单序列化/反序列化
 */
#include "PluginManifest.h"

#include <sstream>
#include <algorithm>

namespace chrono {
namespace client {
namespace plugin {

static std::string type_to_string(PluginType type) {
    switch (type) {
        case PluginType::kCpp:  return "cpp";
        case PluginType::kJs:   return "js";
        case PluginType::kRust: return "rust";
        default: return "unknown";
    }
}

static PluginType string_to_type(const std::string& s) {
    if (s == "cpp")  return PluginType::kCpp;
    if (s == "js")   return PluginType::kJs;
    if (s == "rust") return PluginType::kRust;
    return PluginType::kJs;
}

static uint64_t parse_permissions(const std::vector<std::string>& perms) {
    uint64_t bits = 0;
    for (const auto& p : perms) {
        if (p == "network")      bits |= (uint64_t)PluginPermission::kNetwork;
        else if (p == "filesystem") bits |= (uint64_t)PluginPermission::kFileSystem;
        else if (p == "ipc_send") bits |= (uint64_t)PluginPermission::kIpcSend;
        else if (p == "ipc_receive") bits |= (uint64_t)PluginPermission::kIpcReceive;
        else if (p == "http")    bits |= (uint64_t)PluginPermission::kHttpRegister;
        else if (p == "storage") bits |= (uint64_t)PluginPermission::kStorage;
        else if (p == "crypto")  bits |= (uint64_t)PluginPermission::kCrypto;
        else if (p == "ui")      bits |= (uint64_t)PluginPermission::kWindowUI;
        else if (p == "execute_js") bits |= (uint64_t)PluginPermission::kExecuteJS;
        else if (p == "social")  bits |= (uint64_t)PluginPermission::kSocial;
        else if (p == "ai")      bits |= (uint64_t)PluginPermission::kAIAccess;
    }
    return bits;
}

std::string PluginManifest::to_json() const
{
    std::ostringstream os;
    os << "{";
    os << "\"id\":\"" << id << "\",";
    os << "\"name\":\"" << name << "\",";
    os << "\"version\":\"" << version << "\",";
    os << "\"type\":\"" << type_to_string(type) << "\",";
    os << "\"description\":\"" << description << "\",";
    os << "\"author\":\"" << author << "\",";

    os << "\"permissions\":[";
    for (size_t i = 0; i < 64; i++) {
        if (permissions & (1ULL << i)) {
            // 简化为序列化数值
        }
    }
    os << "],";

    os << "\"dependencies\":[";
    for (size_t i = 0; i < dependencies.size(); i++) {
        if (i > 0) os << ",";
        os << "\"" << dependencies[i] << "\"";
    }
    os << "],";

    os << "\"ipc_types\":[";
    for (size_t i = 0; i < ipc_types.size(); i++) {
        if (i > 0) os << ",";
        os << (int)ipc_types[i];
    }
    os << "],";

    os << "\"http_routes\":[";
    for (size_t i = 0; i < http_routes.size(); i++) {
        if (i > 0) os << ",";
        os << "\"" << http_routes[i] << "\"";
    }
    os << "],";

    os << "\"entry_point\":\"" << entry_point << "\"";
    os << "}";
    return os.str();
}

PluginManifest PluginManifest::from_json(const std::string& json)
{
    PluginManifest m;

    auto extract_string = [&](const std::string& key) -> std::string {
        auto pos = json.find("\"" + key + "\"");
        if (pos == std::string::npos) return "";
        auto val_start = json.find('"', pos + key.length() + 3);
        if (val_start == std::string::npos) return "";
        auto val_end = json.find('"', val_start + 1);
        if (val_end == std::string::npos) return "";
        return json.substr(val_start + 1, val_end - val_start - 1);
    };

    auto extract_array_strings = [&](const std::string& key) -> std::vector<std::string> {
        std::vector<std::string> result;
        auto pos = json.find("\"" + key + "\"");
        if (pos == std::string::npos) return result;
        auto arr_start = json.find('[', pos);
        if (arr_start == std::string::npos) return result;
        auto arr_end = json.find(']', arr_start);
        if (arr_end == std::string::npos) return result;
        std::string content = json.substr(arr_start + 1, arr_end - arr_start - 1);
        size_t s = 0;
        while (true) {
            auto q1 = content.find('"', s);
            if (q1 == std::string::npos) break;
            auto q2 = content.find('"', q1 + 1);
            if (q2 == std::string::npos) break;
            result.push_back(content.substr(q1 + 1, q2 - q1 - 1));
            s = q2 + 1;
        }
        return result;
    };

    auto extract_array_ints = [&](const std::string& key) -> std::vector<uint8_t> {
        std::vector<uint8_t> result;
        auto pos = json.find("\"" + key + "\"");
        if (pos == std::string::npos) return result;
        auto arr_start = json.find('[', pos);
        if (arr_start == std::string::npos) return result;
        auto arr_end = json.find(']', arr_start);
        if (arr_end == std::string::npos) return result;
        std::string content = json.substr(arr_start + 1, arr_end - arr_start - 1);
        size_t s = 0;
        while (s < content.size()) {
            while (s < content.size() && (content[s] == ' ' || content[s] == ',')) s++;
            if (s >= content.size()) break;
            char* end = nullptr;
            long val = strtol(content.c_str() + s, &end, 10);
            if (end == content.c_str() + s) break;
            result.push_back(static_cast<uint8_t>(val));
            s = end - content.c_str();
        }
        return result;
    };

    m.id          = extract_string("id");
    m.name        = extract_string("name");
    m.version     = extract_string("version");
    m.type        = string_to_type(extract_string("type"));
    m.description = extract_string("description");
    m.author      = extract_string("author");
    m.entry_point = extract_string("entry_point");

    auto perm_strings = extract_array_strings("permissions");
    m.permissions = parse_permissions(perm_strings);

    m.dependencies = extract_array_strings("dependencies");
    m.ipc_types    = extract_array_ints("ipc_types");
    m.http_routes  = extract_array_strings("http_routes");

    return m;
}

} // namespace plugin
} // namespace client
} // namespace chrono
