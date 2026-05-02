/**
 * Chrono-shift 插件系统公共类型定义
 */
#ifndef CHRONO_CLIENT_PLUGIN_TYPES_H
#define CHRONO_CLIENT_PLUGIN_TYPES_H

#include <cstdint>
#include <string>
#include <vector>

namespace chrono {
namespace client {
namespace plugin {

/** 插件类型 */
enum class PluginType : uint8_t {
    kCpp  = 0,  // C++ 动态库 (.dll/.so)
    kJs   = 1,  // JavaScript 插件
    kRust = 2,  // Rust FFI 插件
};

/** 插件状态 */
enum class PluginState : uint8_t {
    kUnloaded   = 0,
    kLoading    = 1,
    kRunning    = 2,
    kStopped    = 3,
    kError      = 4,
};

/** 插件权限 */
enum class PluginPermission : uint64_t {
    kNone           = 0,
    kNetwork        = 1 << 0,   // 网络访问
    kFileSystem     = 1 << 1,   // 文件系统
    kIpcSend        = 1 << 2,   // 发送 IPC 消息
    kIpcReceive     = 1 << 3,   // 接收 IPC 消息
    kHttpRegister   = 1 << 4,   // 注册 HTTP 路由
    kStorage        = 1 << 5,   // 访问持久存储
    kCrypto         = 1 << 6,   // 加密/解密
    kWindowUI       = 1 << 7,   // 创建 UI 面板
    kExecuteJS      = 1 << 8,   // 在前端执行 JS
    kSocial         = 1 << 9,   // 社交功能 API
    kAIAccess       = 1 << 10,  // AI 接口
    kAll            = 0xFFFFFFFFFFFFFFFFULL,
};

/** 日志级别 */
enum class LogLevel : uint8_t {
    kDebug   = 0,
    kInfo    = 1,
    kWarning = 2,
    kError   = 3,
};

} // namespace plugin
} // namespace client
} // namespace chrono

#endif // CHRONO_CLIENT_PLUGIN_TYPES_H
