/**
 * DevToolsIpcHandler.h — 开发者模式 IPC 消息处理器
 *
 * 通过 IpcBridge 注册 0xC0-0xC3 扩展消息类型，
 * 实现 JavaScript 前端与 C++ 后端的双向通信。
 *
 * IPC 消息类型:
 * | 类型 | 方向   | 功能                  |
 * |------|--------|----------------------|
 * | 0xC0 | JS→C++ | 启用/禁用开发者模式    |
 * | 0xC1 | JS→C++ | 执行 CLI 命令          |
 * | 0xC2 | C++→JS | 推送日志到 UI 面板     |
 * | 0xC3 | C++→JS | 推送网络事件           |
 */
#ifndef CHRONO_CLIENT_DEVTOOLS_IPC_HANDLER_H
#define CHRONO_CLIENT_DEVTOOLS_IPC_HANDLER_H

#include <cstdint>
#include <string>
#include <vector>

namespace chrono {
namespace client {
namespace app {
    class IpcBridge;
} // namespace app

namespace devtools {

class DevToolsEngine;

/**
 * IPC 消息处理器
 *
 * 管理扩展消息类型 (0xA0-0xEF) 范围内的 DevTools 消息路由。
 * 通过 IpcBridge::register_handler() 注册回调。
 */
class DevToolsIpcHandler {
public:
    /* DevTools IPC 消息类型常量 */
    static constexpr uint8_t kDevToolsEnable      = 0xC0;  // JS→C++ 启用/禁用
    static constexpr uint8_t kDevToolsExec         = 0xC1;  // JS→C++ 执行命令
    static constexpr uint8_t kDevToolsLog          = 0xC2;  // C++→JS 日志推送
    static constexpr uint8_t kDevToolsNetworkEvent = 0xC3;  // C++→JS 网络事件

    explicit DevToolsIpcHandler(DevToolsEngine& engine);
    ~DevToolsIpcHandler() = default;

    /* 禁止拷贝 */
    DevToolsIpcHandler(const DevToolsIpcHandler&) = delete;
    DevToolsIpcHandler& operator=(const DevToolsIpcHandler&) = delete;

    /**
     * 注册 IPC 处理器
     * @param ipc IPC 桥接器引用
     */
    void register_handlers(app::IpcBridge& ipc);

    /**
     * 注销 IPC 处理器
     */
    void unregister_handlers(app::IpcBridge& ipc);

    // ============================================================
    // C++ → JS 推送 (从后端发送事件到 UI 面板)
    // ============================================================

    /**
     * 推送日志消息到前端
     * @param level   日志级别 (info/warn/error)
     * @param message 日志内容
     */
    void push_log(const std::string& level, const std::string& message);

    /**
     * 推送网络事件到前端
     * @param event_type 事件类型 (connected/disconnected/data)
     * @param data       事件数据
     */
    void push_network_event(const std::string& event_type,
                            const std::string& data);

private:
    /* === IPC 消息处理器 === */

    /** 处理 0xC0: 启用/禁用 */
    void on_devtools_enable(const std::string& json_data);

    /** 处理 0xC1: 执行命令 */
    void on_devtools_exec(const std::string& json_data);

    DevToolsEngine& engine_;

    /** 已注册的 IPC 类型列表 */
    std::vector<uint8_t> registered_types_;
};

} // namespace devtools
} // namespace client
} // namespace chrono

#endif // CHRONO_CLIENT_DEVTOOLS_IPC_HANDLER_H
