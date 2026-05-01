/**
 * Chrono-shift 客户端 IPC 桥接实现
 * C++17 重构版
 */

#include "IpcBridge.h"

#include "../util/Logger.h"

namespace chrono {
namespace client {
namespace app {

/* ============================================================
 * 初始化
 * ============================================================ */

void IpcBridge::init(IpcCallback default_callback)
{
    default_callback_ = std::move(default_callback);
    handlers_.clear();
    LOG_INFO("IPC 桥接初始化完成");
}

/* ============================================================
 * 注册消息处理器
 * ============================================================ */

void IpcBridge::register_handler(IpcMessageType type, IpcCallback handler)
{
    if (handlers_.size() >= kMaxHandlers) {
        LOG_ERROR("IPC 处理器注册表已满 (%zu/%zu)",
                  handlers_.size(), kMaxHandlers);
        return;
    }

    handlers_.emplace_back(type, std::move(handler));
    LOG_DEBUG("IPC 处理器已注册: type=0x%02X",
              static_cast<uint8_t>(type));
}

/* ============================================================
 * 发送消息到 JS
 * ============================================================ */

int IpcBridge::send_to_js(void* webview, const std::string& json_str)
{
    (void)webview;
    LOG_DEBUG("发送到 JS: %s", json_str.c_str());
    /* Phase 2 实现通过 WebView2 执行 JS:
     *   ICoreWebView2* wv = static_cast<ICoreWebView2*>(webview);
     *   wv->ExecuteScript(json_str.c_str(), nullptr);
     */
    return 0;
}

/* ============================================================
 * 处理来自 JS 的消息
 * ============================================================ */

int IpcBridge::handle_from_js(const std::string& json_str)
{
    if (json_str.empty()) {
        return -1;
    }

    LOG_DEBUG("收到 JS 消息: %s", json_str.c_str());

    /* 查找注册的处理器 */
    for (const auto& [type, handler] : handlers_) {
        if (handler) {
            handler(type, json_str);
            return 0;
        }
    }

    /* 默认处理器 */
    if (default_callback_) {
        /* 尝试从 JSON 中解析 type 字段 */
        // 简化处理：传递 kSystemNotify 作为默认类型
        default_callback_(IpcMessageType::kSystemNotify, json_str);
    }

    return 0;
}

/* ============================================================
 * 类型转换工具
 * ============================================================ */

uint8_t IpcBridge::type_to_value(IpcMessageType type)
{
    return static_cast<uint8_t>(type);
}

IpcMessageType IpcBridge::value_to_type(uint8_t value)
{
    switch (value) {
        case 0x01: return IpcMessageType::kLogin;
        case 0x02: return IpcMessageType::kLogout;
        case 0x10: return IpcMessageType::kSendMessage;
        case 0x11: return IpcMessageType::kGetMessages;
        case 0x20: return IpcMessageType::kGetContacts;
        case 0x30: return IpcMessageType::kGetTemplates;
        case 0x31: return IpcMessageType::kApplyTemplate;
        case 0x40: return IpcMessageType::kFileUpload;
        case 0x50: return IpcMessageType::kOpenUrl;
        case 0x60: return IpcMessageType::kConnected;
        case 0x61: return IpcMessageType::kDisconnected;
        case 0xFF: return IpcMessageType::kSystemNotify;
        default:   return IpcMessageType::kSystemNotify;
    }
}

} // namespace app
} // namespace client
} // namespace chrono
