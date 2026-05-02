/**
 * DevToolsIpcHandler.cpp — IPC 消息处理器实现
 *
 * 处理 0xC0-0xC3 扩展消息类型的 IPC 消息，
 * 在 JavaScript 前端与 C++ DevToolsEngine 之间路由。
 */
#include "DevToolsIpcHandler.h"
#include "DevToolsEngine.h"

#include "app/IpcBridge.h"
#include "util/Logger.h"

#include <sstream>

namespace chrono {
namespace client {
namespace devtools {

/* ============================================================
 * 构造函数
 * ============================================================ */

DevToolsIpcHandler::DevToolsIpcHandler(DevToolsEngine& engine)
    : engine_(engine)
{
}

/* ============================================================
 * 处理器注册
 * ============================================================ */

void DevToolsIpcHandler::register_handlers(app::IpcBridge& ipc)
{
    using namespace app;

    /* 0xC0: DEV_TOOLS_ENABLE — 启用/禁用开发者模式 */
    ipc.register_handler(
        static_cast<IpcMessageType>(kDevToolsEnable),
        [this](IpcMessageType /*type*/, const std::string& json_data)
        {
            this->on_devtools_enable(json_data);
        });

    /* 0xC1: DEV_TOOLS_EXEC — 执行 CLI 命令 */
    ipc.register_handler(
        static_cast<IpcMessageType>(kDevToolsExec),
        [this](IpcMessageType /*type*/, const std::string& json_data)
        {
            this->on_devtools_exec(json_data);
        });

    /* 0xC2 和 0xC3 是 C++→JS 方向，不需要注册处理器 */

    registered_types_ = {kDevToolsEnable, kDevToolsExec};
    LOG_INFO("DevToolsIpcHandler: 已注册 IPC 处理器 (0xC0-0xC1)");
}

void DevToolsIpcHandler::unregister_handlers(app::IpcBridge& /*ipc*/)
{
    /* IpcBridge 当前不提供按类型注销接口 */
    registered_types_.clear();
    LOG_INFO("DevToolsIpcHandler: IPC 处理器已注销");
}

/* ============================================================
 * IPC 消息处理
 * ============================================================ */

void DevToolsIpcHandler::on_devtools_enable(const std::string& json_data)
{
    /* 解析 {"enable": true/false} */
    bool enable = (json_data.find("\"enable\":true") != std::string::npos) ||
                  (json_data.find("\"enable\": true") != std::string::npos);

    engine_.set_dev_mode_enabled(enable);
    LOG_INFO("DevTools IPC: 开发者模式 %s", enable ? "启用" : "禁用");
}

void DevToolsIpcHandler::on_devtools_exec(const std::string& json_data)
{
    /* 提取 cmd 字段 */
    std::string cmd;
    auto pos = json_data.find("\"cmd\"");
    if (pos != std::string::npos) {
        auto vstart = json_data.find('"', pos + 5);
        if (vstart != std::string::npos) {
            auto vend = json_data.find('"', vstart + 1);
            if (vend != std::string::npos) {
                cmd = json_data.substr(vstart + 1, vend - vstart - 1);
            }
        }
    }

    std::string result;
    if (!cmd.empty()) {
        result = engine_.execute_command(cmd);
        LOG_INFO("DevTools IPC: 执行命令 '%s' (%zu 字节输出)",
                 cmd.c_str(), result.size());
    } else {
        result = "Missing 'cmd' field\n";
        LOG_WARN("DevTools IPC: 收到空命令");
    }
}

/* ============================================================
 * C++ → JS 推送
 * ============================================================ */

void DevToolsIpcHandler::push_log(const std::string& level,
                                  const std::string& message)
{
    /* 对日志消息做 JSON 转义 */
    auto escape = [](const std::string& s) -> std::string {
        std::string out;
        for (char c : s) {
            switch (c) {
                case '"':  out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n";  break;
                case '\r': out += "\\r";  break;
                case '\t': out += "\\t";  break;
                default:   out += c;      break;
            }
        }
        return out;
    };

    std::string json =
        "{\"type\":\"devtools_log\",\"level\":\""
        + escape(level) + "\",\"message\":\""
        + escape(message) + "\"}";

    /* 通过 IpcBridge::send_to_js 发送 — 需要 WebView 指针 */
    /* 实际调用由 AppContext 管理，此处仅构造消息 */
    (void)json;
}

void DevToolsIpcHandler::push_network_event(const std::string& event_type,
                                            const std::string& data)
{
    auto escape = [](const std::string& s) -> std::string {
        std::string out;
        for (char c : s) {
            switch (c) {
                case '"':  out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n";  break;
                case '\r': out += "\\r";  break;
                case '\t': out += "\\t";  break;
                default:   out += c;      break;
            }
        }
        return out;
    };

    std::string json =
        "{\"type\":\"devtools_network_event\",\"event\":\""
        + escape(event_type) + "\",\"data\":\""
        + escape(data) + "\"}";

    (void)json;
}

} // namespace devtools
} // namespace client
} // namespace chrono
