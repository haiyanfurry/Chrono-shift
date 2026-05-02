/**
 * Chrono-shift 插件管理器
 *
 * 管理插件的加载、注册、生命周期和通信
 */
#ifndef CHRONO_CLIENT_PLUGIN_MANAGER_H
#define CHRONO_CLIENT_PLUGIN_MANAGER_H

#include "PluginInterface.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>

namespace chrono {
namespace client {

// 前向声明
namespace app {
    class IpcBridge;
    class ClientHttpServer;
}

namespace plugin {

/** 插件事件回调 */
using PluginEventHandler = std::function<void(const std::string& plugin_id,
                                               PluginState state)>;

/** 插件管理器 */
class PluginManager {
public:
    static constexpr size_t kMaxPlugins = 64;

    PluginManager();
    ~PluginManager();

    /* 禁止拷贝 */
    PluginManager(const PluginManager&) = delete;
    PluginManager& operator=(const PluginManager&) = delete;

    // ============================================================
    // 生命周期
    // ============================================================

    /** 初始化（设置插件目录、创建必要目录） */
    int init(const std::string& plugins_dir,
             app::IpcBridge* ipc,
             app::ClientHttpServer* http);

    /** 加载所有插件 */
    int load_all();

    /** 启动所有已加载的插件 */
    int start_all();

    /** 停止所有插件 */
    int stop_all();

    /** 卸载所有插件 */
    int unload_all();

    // ============================================================
    // 单个插件管理
    // ============================================================

    /** 加载单个插件 (通过 manifest.json 路径) */
    bool load_plugin(const std::string& manifest_path);

    /** 加载单个插件 (通过已解析的 manifest) */
    bool load_plugin(const PluginManifest& manifest);

    /** 卸载单个插件 */
    bool unload_plugin(const std::string& plugin_id);

    /** 启动单个插件 */
    bool start_plugin(const std::string& plugin_id);

    /** 停止单个插件 */
    bool stop_plugin(const std::string& plugin_id);

    /** 获取插件 */
    IPlugin* get_plugin(const std::string& plugin_id);

    /** 检查插件是否已加载 */
    bool is_loaded(const std::string& plugin_id) const;

    // ============================================================
    // 查询
    // ============================================================

    /** 列出所有已加载插件 */
    std::vector<PluginInfo> list_plugins() const;

    /** 获取插件数量 */
    size_t plugin_count() const { return plugins_.size(); }

    /** 设置插件事件监听 */
    void set_event_handler(PluginEventHandler handler) {
        event_handler_ = std::move(handler);
    }

    /** 获取插件目录路径 */
    const std::string& plugins_dir() const { return plugins_dir_; }

    // ============================================================
    // 插件间通信
    // ============================================================

    /** 向插件发送 IPC 消息 */
    int send_to_plugin(const std::string& plugin_id,
                       uint8_t type, const std::string& data);

    /** 广播消息给所有插件 */
    int broadcast(uint8_t type, const std::string& data);

private:
    /** 扫描插件目录 */
    std::vector<std::string> scan_plugins();

    /** 从 manifest 文件解析 */
    PluginManifest parse_manifest(const std::string& manifest_path);

    /** 加载 C++ 动态库插件 */
    IPlugin* load_cpp_plugin(const PluginManifest& manifest);

    /** 加载 JS 插件 */
    IPlugin* load_js_plugin(const PluginManifest& manifest);

    /** 触发事件 */
    void fire_event(const std::string& plugin_id, PluginState state);

    /** 将 JS 插件代码注入前端 */
    int inject_js_plugin(const std::string& plugin_id, const std::string& js_code);

private:
    std::unordered_map<std::string, std::unique_ptr<IPlugin>> plugins_;
    std::unordered_map<std::string, PluginInfo> plugin_infos_;

    std::string plugins_dir_;
    app::IpcBridge*        ipc_;
    app::ClientHttpServer* http_;
    bool initialized_;

    PluginEventHandler event_handler_;
};

} // namespace plugin
} // namespace client
} // namespace chrono

#endif // CHRONO_CLIENT_PLUGIN_MANAGER_H
