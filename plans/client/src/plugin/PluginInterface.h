/**
 * Chrono-shift 插件基类接口
 *
 * 所有插件必须实现 IPlugin 接口:
 * - C++ 插件: 导出 CreatePlugin/DestroyPlugin 函数
 * - JS 插件: 通过 PluginAPI 注册
 * - Rust 插件: 通过 FFI 导出
 */
#ifndef CHRONO_CLIENT_PLUGIN_INTERFACE_H
#define CHRONO_CLIENT_PLUGIN_INTERFACE_H

#include "PluginManifest.h"

namespace chrono {
namespace client {
namespace plugin {

/** 插件基类 */
class IPlugin {
public:
    virtual ~IPlugin() = default;

    /** 获取插件清单 */
    virtual const PluginManifest& manifest() const = 0;

    /** 初始化插件（加载配置、分配资源） */
    virtual bool init() = 0;

    /** 启动插件（开始监听、注册路由等） */
    virtual bool start() = 0;

    /** 停止插件 */
    virtual bool stop() = 0;

    /** 获取当前状态 */
    virtual PluginState state() const = 0;
};

/** C++ 插件抽象基类 */
class CppPlugin : public IPlugin {
public:
    explicit CppPlugin(PluginManifest manifest)
        : manifest_(std::move(manifest)), state_(PluginState::kUnloaded) {}

    const PluginManifest& manifest() const override { return manifest_; }
    PluginState state() const override { return state_; }

    bool init() override { state_ = PluginState::kLoading; return true; }
    bool start() override { state_ = PluginState::kRunning; return true; }
    bool stop() override { state_ = PluginState::kStopped; return true; }

protected:
    PluginManifest manifest_;
    PluginState    state_;
};

/** JS 插件 */
class JsPlugin : public IPlugin {
public:
    explicit JsPlugin(PluginManifest manifest, std::string js_code)
        : manifest_(std::move(manifest)), js_code_(std::move(js_code)),
          state_(PluginState::kUnloaded) {}

    const PluginManifest& manifest() const override { return manifest_; }
    PluginState state() const override { return state_; }
    const std::string& js_code() const { return js_code_; }

    bool init() override { state_ = PluginState::kLoading; return true; }
    bool start() override { state_ = PluginState::kRunning; return true; }
    bool stop() override { state_ = PluginState::kStopped; return true; }

private:
    PluginManifest manifest_;
    std::string    js_code_;
    PluginState    state_;
};

/** C++ 插件导出函数类型 */
using CreatePluginFunc = IPlugin* (*)();
using DestroyPluginFunc = void (*)(IPlugin*);

} // namespace plugin
} // namespace client
} // namespace chrono

#endif // CHRONO_CLIENT_PLUGIN_INTERFACE_H
