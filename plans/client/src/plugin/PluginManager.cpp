/**
 * Chrono-shift 插件管理器实现
 */
#include "PluginManager.h"

#include "../util/Logger.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#else
#include <dlfcn.h>
#include <dirent.h>
#include <sys/stat.h>
#endif

namespace chrono {
namespace client {
namespace plugin {

PluginManager::PluginManager()
    : ipc_(nullptr)
    , http_(nullptr)
    , initialized_(false)
{
}

PluginManager::~PluginManager()
{
    stop_all();
    unload_all();
}

int PluginManager::init(const std::string& plugins_dir,
                        app::IpcBridge* ipc,
                        app::ClientHttpServer* http)
{
    plugins_dir_ = plugins_dir;
    ipc_ = ipc;
    http_ = http;
    initialized_ = true;

    LOG_INFO("插件管理器初始化, 插件目录: %s", plugins_dir_.c_str());
    return 0;
}

// ============================================================
// 插件扫描
// ============================================================

std::vector<std::string> PluginManager::scan_plugins()
{
    std::vector<std::string> manifests;
#ifdef _WIN32
    std::string search_path = plugins_dir_ + "/*/manifest.json";
    WIN32_FIND_DATAA find_data;
    HANDLE hFind = FindFirstFileA(search_path.c_str(), &find_data);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            manifests.push_back(plugins_dir_ + "/" + find_data.cFileName);
        } while (FindNextFileA(hFind, &find_data));
        FindClose(hFind);
    }
#else
    DIR* dir = opendir(plugins_dir_.c_str());
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (entry->d_type == DT_DIR) {
                std::string name(entry->d_name);
                if (name != "." && name != "..") {
                    std::string manifest_path = plugins_dir_ + "/" + name + "/manifest.json";
                    struct stat st;
                    if (stat(manifest_path.c_str(), &st) == 0) {
                        manifests.push_back(manifest_path);
                    }
                }
            }
        }
        closedir(dir);
    }
#endif
    LOG_INFO("扫描到 %zu 个插件清单", manifests.size());
    return manifests;
}

PluginManifest PluginManager::parse_manifest(const std::string& manifest_path)
{
    std::ifstream ifs(manifest_path);
    if (!ifs.is_open()) {
        LOG_ERROR("无法打开插件清单: %s", manifest_path.c_str());
        return PluginManifest();
    }
    std::stringstream buffer;
    buffer << ifs.rdbuf();
    return PluginManifest::from_json(buffer.str());
}

// ============================================================
// 加载/卸载
// ============================================================

bool PluginManager::load_plugin(const std::string& manifest_path)
{
    if (!initialized_) {
        LOG_ERROR("插件管理器未初始化");
        return false;
    }

    PluginManifest manifest = parse_manifest(manifest_path);
    if (manifest.id.empty()) {
        LOG_ERROR("插件清单解析失败: %s", manifest_path.c_str());
        return false;
    }

    return load_plugin(manifest);
}

bool PluginManager::load_plugin(const PluginManifest& manifest)
{
    if (plugins_.size() >= kMaxPlugins) {
        LOG_ERROR("插件数量已达上限 (%zu)", kMaxPlugins);
        return false;
    }

    if (plugins_.find(manifest.id) != plugins_.end()) {
        LOG_WARN("插件已存在: %s", manifest.id.c_str());
        return false;
    }

    auto start_time = std::chrono::steady_clock::now();

    IPlugin* plugin = nullptr;
    switch (manifest.type) {
        case PluginType::kCpp:
            plugin = load_cpp_plugin(manifest);
            break;
        case PluginType::kJs:
            plugin = load_js_plugin(manifest);
            break;
        default:
            LOG_ERROR("不支持的插件类型: %d", (int)manifest.type);
            return false;
    }

    if (!plugin) {
        LOG_ERROR("插件加载失败: %s", manifest.id.c_str());
        return false;
    }

    auto end_time = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();

    PluginInfo info;
    info.manifest    = manifest;
    info.state       = PluginState::kUnloaded;
    info.handle      = nullptr;
    info.base_path   = plugins_dir_ + "/" + manifest.id;
    info.load_time_ms = elapsed;

    plugins_[manifest.id] = std::unique_ptr<IPlugin>(plugin);
    plugin_infos_[manifest.id] = info;

    LOG_INFO("插件加载成功: %s v%s (%zu ms)",
             manifest.id.c_str(), manifest.version.c_str(), elapsed);
    fire_event(manifest.id, PluginState::kLoading);
    return true;
}

bool PluginManager::unload_plugin(const std::string& plugin_id)
{
    auto it = plugins_.find(plugin_id);
    if (it == plugins_.end()) {
        LOG_WARN("插件未找到: %s", plugin_id.c_str());
        return false;
    }

    it->second->stop();
    fire_event(plugin_id, PluginState::kUnloaded);

    plugins_.erase(it);
    plugin_infos_.erase(plugin_id);

    LOG_INFO("插件已卸载: %s", plugin_id.c_str());
    return true;
}

// ============================================================
// 启动/停止
// ============================================================

bool PluginManager::start_plugin(const std::string& plugin_id)
{
    auto it = plugins_.find(plugin_id);
    if (it == plugins_.end()) return false;

    if (!it->second->init()) {
        LOG_ERROR("插件初始化失败: %s", plugin_id.c_str());
        plugin_infos_[plugin_id].state = PluginState::kError;
        return false;
    }

    if (!it->second->start()) {
        LOG_ERROR("插件启动失败: %s", plugin_id.c_str());
        plugin_infos_[plugin_id].state = PluginState::kError;
        return false;
    }

    plugin_infos_[plugin_id].state = PluginState::kRunning;

    /* 如果是 JS 插件，注入到前端 */
    auto* js_plugin = dynamic_cast<JsPlugin*>(it->second.get());
    if (js_plugin) {
        inject_js_plugin(plugin_id, js_plugin->js_code());
    }

    LOG_INFO("插件已启动: %s", plugin_id.c_str());
    fire_event(plugin_id, PluginState::kRunning);
    return true;
}

bool PluginManager::stop_plugin(const std::string& plugin_id)
{
    auto it = plugins_.find(plugin_id);
    if (it == plugins_.end()) return false;

    it->second->stop();
    plugin_infos_[plugin_id].state = PluginState::kStopped;

    LOG_INFO("插件已停止: %s", plugin_id.c_str());
    fire_event(plugin_id, PluginState::kStopped);
    return true;
}

// ============================================================
// 批量操作
// ============================================================

int PluginManager::load_all()
{
    auto manifests = scan_plugins();
    int count = 0;
    for (const auto& path : manifests) {
        if (load_plugin(path)) count++;
    }
    LOG_INFO("批量加载完成: %d/%zu", count, manifests.size());
    return count;
}

int PluginManager::start_all()
{
    int count = 0;
    for (auto& [id, _] : plugins_) {
        if (start_plugin(id)) count++;
    }
    return count;
}

int PluginManager::stop_all()
{
    int count = 0;
    for (auto& [id, _] : plugins_) {
        if (stop_plugin(id)) count++;
    }
    return count;
}

int PluginManager::unload_all()
{
    int count = 0;
    while (!plugins_.empty()) {
        if (unload_plugin(plugins_.begin()->first)) count++;
    }
    return count;
}

// ============================================================
// 查询
// ============================================================

IPlugin* PluginManager::get_plugin(const std::string& plugin_id)
{
    auto it = plugins_.find(plugin_id);
    return (it != plugins_.end()) ? it->second.get() : nullptr;
}

bool PluginManager::is_loaded(const std::string& plugin_id) const
{
    return plugins_.find(plugin_id) != plugins_.end();
}

std::vector<PluginInfo> PluginManager::list_plugins() const
{
    std::vector<PluginInfo> result;
    for (const auto& [id, info] : plugin_infos_) {
        result.push_back(info);
    }
    return result;
}

// ============================================================
// 插件间通信
// ============================================================

int PluginManager::send_to_plugin(const std::string& plugin_id,
                                  uint8_t type, const std::string& data)
{
    (void)plugin_id;
    (void)type;
    (void)data;
    // Phase 3: 实现插件间消息路由
    LOG_DEBUG("插件消息: %s type=0x%02X", plugin_id.c_str(), type);
    return 0;
}

int PluginManager::broadcast(uint8_t type, const std::string& data)
{
    (void)type;
    (void)data;
    // Phase 3: 广播给所有插件
    return 0;
}

// ============================================================
// 内部加载器
// ============================================================

IPlugin* PluginManager::load_cpp_plugin(const PluginManifest& manifest)
{
#ifdef _WIN32
    std::string dll_path = plugins_dir_ + "/" + manifest.id + "/" + manifest.entry_point;
    HMODULE hModule = LoadLibraryA(dll_path.c_str());
    if (!hModule) {
        LOG_ERROR("无法加载 DLL: %s (err=%lu)", dll_path.c_str(), GetLastError());
        return nullptr;
    }

    auto create_fn = (CreatePluginFunc)GetProcAddress(hModule, "CreatePlugin");
    if (!create_fn) {
        LOG_ERROR("DLL 中未找到 CreatePlugin 导出: %s", dll_path.c_str());
        FreeLibrary(hModule);
        return nullptr;
    }

    IPlugin* plugin = create_fn();
    if (!plugin) {
        LOG_ERROR("CreatePlugin 返回空: %s", dll_path.c_str());
        FreeLibrary(hModule);
        return nullptr;
    }

    // 记录句柄
    plugin_infos_[manifest.id].handle = hModule;
    return plugin;
#else
    std::string so_path = plugins_dir_ + "/" + manifest.id + "/" + manifest.entry_point;
    void* handle = dlopen(so_path.c_str(), RTLD_NOW);
    if (!handle) {
        LOG_ERROR("无法加载 SO: %s (%s)", so_path.c_str(), dlerror());
        return nullptr;
    }

    auto create_fn = (CreatePluginFunc)dlsym(handle, "CreatePlugin");
    if (!create_fn) {
        LOG_ERROR("SO 中未找到 CreatePlugin: %s", dlerror());
        dlclose(handle);
        return nullptr;
    }

    IPlugin* plugin = create_fn();
    plugin_infos_[manifest.id].handle = handle;
    return plugin;
#endif
}

IPlugin* PluginManager::load_js_plugin(const PluginManifest& manifest)
{
    std::string js_path = plugins_dir_ + "/" + manifest.id + "/" + manifest.entry_point;
    std::ifstream ifs(js_path);
    if (!ifs.is_open()) {
        LOG_ERROR("无法打开 JS 插件文件: %s", js_path.c_str());
        return nullptr;
    }

    std::stringstream buffer;
    buffer << ifs.rdbuf();

    return new JsPlugin(manifest, buffer.str());
}

int PluginManager::inject_js_plugin(const std::string& plugin_id,
                                    const std::string& js_code)
{
    // Phase 3: 通过 WebView2 ExecuteScript 注入
    (void)plugin_id;
    (void)js_code;
    LOG_DEBUG("注入 JS 插件: %s (%zu bytes)", plugin_id.c_str(), js_code.size());
    return 0;
}

void PluginManager::fire_event(const std::string& plugin_id, PluginState state)
{
    if (event_handler_) {
        event_handler_(plugin_id, state);
    }
}

} // namespace plugin
} // namespace client
} // namespace chrono
