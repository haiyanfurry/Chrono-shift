/**
 * DevToolsEngine.h — 开发者模式后端引擎
 *
 * 管理开发者模式生命周期，提供：
 * 1. CLI 命令执行封装（通过 CommandHandler 调用）
 * 2. HTTP API 路由注册（通过 ClientHttpServer）
 * 3. IPC 消息处理器注册（通过 IpcBridge）
 * 4. AppContext 模块访问（NetworkClient / LocalStorage / CryptoEngine）
 *
 * 使用方式（在 AppContext::init 中）：
 *   auto& engine = DevToolsEngine::instance();
 *   engine.init(ctx);
 *   engine.register_http_routes(ctx.http_server());
 *   engine.register_ipc_handlers(ctx.ipc());
 */
#ifndef CHRONO_CLIENT_DEVTOOLS_ENGINE_H
#define CHRONO_CLIENT_DEVTOOLS_ENGINE_H

#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <functional>
#include <sstream>
#include <memory>

#include "devtools_cli.h"   // CommandEntry, CommandHandler, DevToolsConfig

namespace chrono {
namespace client {
namespace devtools {

    class DevToolsHttpApi;

} // namespace devtools

namespace app {
    class AppContext;
    class ClientHttpServer;
    class IpcBridge;
} // namespace app
} // namespace client
} // namespace chrono

namespace chrono {
namespace client {
namespace devtools {

/**
 * 开发者模式引擎 — 单例
 *
 * 将独立 CLI 的命令注册表 + 配置封装为 C++ 类，
 * 集成到主应用的 AppContext 生命周期中。
 */
class DevToolsEngine {
public:
    /** 获取单例实例 */
    static DevToolsEngine& instance();

    ~DevToolsEngine();

    /* 禁止拷贝 */
    DevToolsEngine(const DevToolsEngine&) = delete;
    DevToolsEngine& operator=(const DevToolsEngine&) = delete;

    // ============================================================
    // 生命周期
    // ============================================================

    /**
     * 初始化引擎
     * @param ctx 应用上下文引用
     * @return 0=成功, -1=失败
     */
    int init(app::AppContext& ctx);

    /** 关闭引擎 */
    void shutdown();

    /** 引擎是否已初始化 */
    bool is_initialized() const { return initialized_; }

    // ============================================================
    // 开发者模式开关
    // ============================================================

    /** 开发者模式是否已启用 */
    bool is_dev_mode_enabled() const { return enabled_; }

    /** 启用开发者模式 */
    void set_dev_mode_enabled(bool enabled);

    // ============================================================
    // CLI 命令执行
    // ============================================================

    /**
     * 执行 CLI 命令
     * @param cmd_line 命令字符串 (如 "health" / "token eyJ...")
     * @return 命令执行的文本输出
     *
     * 实现：将 cmd_line 分词 → 查找 CommandHandler → 捕获 stdout 输出
     */
    std::string execute_command(const std::string& cmd_line);

    /**
     * 执行 CLI 命令 (带参数数组)
     * @param argc 参数个数
     * @param argv 参数数组
     * @return 命令执行的文本输出
     */
    std::string execute_command(int argc, char** argv);

    // ============================================================
    // HTTP API 路由注册
    // ============================================================

    /**
     * 向 ClientHttpServer 注册 /api/devtools/* 路由
     * @param http_server HTTP 服务器引用
     */
    void register_http_routes(app::ClientHttpServer& http_server);

    /**
     * 注销 HTTP 路由
     */
    void unregister_http_routes(app::ClientHttpServer& http_server);

    // ============================================================
    // IPC 消息处理器注册
    // ============================================================

    /**
     * 向 IpcBridge 注册 0xC0-0xC3 扩展消息处理器
     * @param ipc IPC 桥接器引用
     */
    void register_ipc_handlers(app::IpcBridge& ipc);

    /**
     * 注销 IPC 处理器
     */
    void unregister_ipc_handlers(app::IpcBridge& ipc);

    // ============================================================
    // 命令查询
    // ============================================================

    /** 获取所有已注册命令的 JSON 描述 */
    std::string get_commands_json() const;

    /** 获取引擎状态 JSON */
    std::string get_status_json() const;

    /** 获取 AppContext 引用 (仅供 DevToolsHttpApi/IpcHandler 使用) */
    app::AppContext* context() { return ctx_; }

private:
    DevToolsEngine();

    /** 输出捕获回调 — 用于 execute_command 收集 stdout */
    static int output_capture(const char* buf, int len);

    /** 当前捕获的字符串流 */
    static std::ostringstream* s_capture_stream;

    /** 初始化标志 */
    std::atomic<bool> initialized_{false};

    /** 开发者模式启用标志 */
    std::atomic<bool> enabled_{false};

    /** AppContext 指针 */
    app::AppContext* ctx_ = nullptr;

    /** 已注册的 IPC 处理器 ID 列表 (用于注销) */
    std::vector<uint8_t> registered_ipc_types_;

    /** HTTP API 处理器 (由 register_http_routes 创建) */
    std::unique_ptr<DevToolsHttpApi> http_api_;
};

} // namespace devtools
} // namespace client
} // namespace chrono

#endif // CHRONO_CLIENT_DEVTOOLS_ENGINE_H
