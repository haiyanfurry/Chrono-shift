/**
 * Chrono-shift 插件清单定义
 *
 * 插件清单 (manifest.json) 结构
 * {
 *   "id": "com.example.myplugin",
 *   "name": "我的插件",
 *   "version": "1.0.0",
 *   "type": "cpp|js|rust",
 *   "description": "插件功能描述",
 *   "author": "作者名",
 *   "permissions": ["network", "storage", "ipc_send"],
 *   "dependencies": ["com.example.base"],
 *   "ipc_types": [0x70, 0x71, 0x72],
 *   "http_routes": ["/api/plugins/myplugin/"]
 * }
 */
#ifndef CHRONO_CLIENT_PLUGIN_MANIFEST_H
#define CHRONO_CLIENT_PLUGIN_MANIFEST_H

#include "types.h"

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_set>

namespace chrono {
namespace client {
namespace plugin {

/** 插件清单 */
struct PluginManifest {
    std::string              id;               // 唯一标识 (如 "com.example.myplugin")
    std::string              name;             // 显示名称
    std::string              version;          // 语义化版本 "1.0.0"
    PluginType               type;             // 插件类型
    std::string              description;      // 描述
    std::string              author;           // 作者
    uint64_t                 permissions;      // 位掩码 (PluginPermission)
    std::vector<std::string> dependencies;     // 依赖插件 ID 列表
    std::vector<uint8_t>     ipc_types;        // 监听的 IPC 消息类型
    std::vector<std::string> http_routes;      // HTTP 路由前缀
    std::string              entry_point;      // 入口文件 (对 C++ 是 .dll, 对 JS 是 .js)

    /** 检查是否拥有某权限 */
    bool has_permission(PluginPermission perm) const {
        return (permissions & static_cast<uint64_t>(perm)) != 0;
    }

    /** JSON 序列化 */
    std::string to_json() const;

    /** 从 JSON 字符串解析 */
    static PluginManifest from_json(const std::string& json);
};

/** 插件信息（运行时） */
struct PluginInfo {
    PluginManifest manifest;
    PluginState    state;
    void*          handle;   // DLL 句柄或 JS 上下文指针
    std::string    base_path; // 插件目录路径
    uint64_t       load_time_ms; // 加载耗时
};

} // namespace plugin
} // namespace client
} // namespace chrono

#endif // CHRONO_CLIENT_PLUGIN_MANIFEST_H
