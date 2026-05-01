/**
 * Chrono-shift 客户端 IPC 桥接
 * C++17 重构版
 *
 * 实现前端 JavaScript 与 C++ 后端的双向通信
 * 支持消息类型注册、JSON 消息分发、JS 调用发送
 */
#ifndef CHRONO_CLIENT_IPC_BRIDGE_H
#define CHRONO_CLIENT_IPC_BRIDGE_H

#include <cstdint>
#include <string>
#include <functional>
#include <unordered_map>
#include <vector>

namespace chrono {
namespace client {
namespace app {

/**
 * IPC 消息类型 (与前端 ipc.js 的 IPC.MessageType 对应)
 */
enum class IpcMessageType : uint8_t {
    kLogin          = 0x01,  // 登录
    kLogout         = 0x02,  // 登出
    kSendMessage    = 0x10,  // 发送消息
    kGetMessages    = 0x11,  // 获取消息列表
    kGetContacts    = 0x20,  // 获取联系人
    kGetTemplates   = 0x30,  // 获取模板
    kApplyTemplate  = 0x31,  // 应用模板
    kFileUpload     = 0x40,  // 文件上传
    kOpenUrl        = 0x50,  // 打开外部 URL
    kConnected      = 0x60,  // 连接状态通知
    kDisconnected   = 0x61,  // 断开连接通知
    kSystemNotify   = 0xFF   // 系统通知
};

/**
 * IPC 消息处理器回调
 * @param type      消息类型
 * @param json_data JSON 格式的消息数据
 */
using IpcCallback = std::function<void(IpcMessageType type, const std::string& json_data)>;

/**
 * IPC 桥接器
 *
 * 管理 C++ 后端与 JavaScript 前端之间的消息路由
 * JS → C++: handle_from_js() 分发到注册的处理器
 * C++ → JS: send_to_js()  通过 WebView2 执行 JS
 */
class IpcBridge {
public:
    /** 最大注册处理器数量 */
    static constexpr size_t kMaxHandlers = 32;

    IpcBridge() = default;
    ~IpcBridge() = default;

    /* 禁止拷贝，允许移动 */
    IpcBridge(const IpcBridge&) = delete;
    IpcBridge& operator=(const IpcBridge&) = delete;
    IpcBridge(IpcBridge&&) = default;
    IpcBridge& operator=(IpcBridge&&) = default;

    // ============================================================
    // 初始化与注册
    // ============================================================

    /**
     * 初始化 IPC 桥接
     * @param default_callback 默认消息处理器 (无特定注册时调用)
     */
    void init(IpcCallback default_callback = nullptr);

    /**
     * 注册特定类型的消息处理器
     * @param type     消息类型
     * @param handler  处理回调
     */
    void register_handler(IpcMessageType type, IpcCallback handler);

    // ============================================================
    // 消息收发
    // ============================================================

    /**
     * 发送消息到 JavaScript 前端
     * @param webview   WebView2 控制器指针 (void* 以避免直接依赖 WebView2 头)
     * @param json_str  JSON 格式的消息
     * @return 0=成功, -1=失败
     */
    int send_to_js(void* webview, const std::string& json_str);

    /**
     * 处理来自 JavaScript 前端的消息
     * @param json_str  JSON 格式的消息
     * @return 0=成功, -1=失败
     */
    int handle_from_js(const std::string& json_str);

    /**
     * 将 IpcMessageType 转换为整数值 (用于与前端通信)
     */
    static uint8_t type_to_value(IpcMessageType type);

    /**
     * 将整数值转换为 IpcMessageType
     */
    static IpcMessageType value_to_type(uint8_t value);

private:
    /** 注册的处理器列表 */
    std::vector<std::pair<IpcMessageType, IpcCallback>> handlers_;

    /** 默认处理器 */
    IpcCallback default_callback_;
};

} // namespace app
} // namespace client
} // namespace chrono

#endif // CHRONO_CLIENT_IPC_BRIDGE_H
