/**
 * Chrono-shift 客户端应用上下文
 * C++17 重构版
 *
 * 管理所有模块的生命周期，作为全局单例使用。
 */
#ifndef CHRONO_CLIENT_APP_CONTEXT_H
#define CHRONO_CLIENT_APP_CONTEXT_H

#include <string>
#include <memory>
#include <atomic>

#include "../util/Logger.h"
#include "../network/NetworkClient.h"
#include "../storage/LocalStorage.h"
#include "../storage/SessionManager.h"
#include "../security/CryptoEngine.h"
#include "../security/TokenManager.h"
#include "IpcBridge.h"
#include "WebViewManager.h"
#include "ClientHttpServer.h"
#include "Updater.h"
#include "../plugin/PluginManager.h"
#include "../ai/AIProvider.h"
#include "../ai/AIChatSession.h"

namespace chrono {
namespace client {
namespace app {

/**
 * 客户端配置
 */
struct ClientConfig {
    std::string server_host     = "127.0.0.1";
    uint16_t    server_port     = 4443;
    std::string app_data_path   = "./data";
    util::LogLevel log_level    = util::LogLevel::kInfo;
    bool        auto_reconnect  = true;
};

/**
 * 应用上下文 — 管理所有模块的生命周期
 */
class AppContext {
public:
    static AppContext& instance();

    ~AppContext();

    AppContext(const AppContext&) = delete;
    AppContext& operator=(const AppContext&) = delete;

    /**
     * 初始化所有模块
     * @param config 客户端配置
     * @return 0 成功, -1 失败
     */
    int init(const ClientConfig& config);

    /** 启动应用 (进入消息循环) */
    int run();

    /** 停止应用 */
    void stop();

    /** 是否正在运行 */
    bool is_running() const { return running_.load(); }

    // ---- 模块访问 ----

    util::LogLevel          log_level()   const { return config_.log_level; }
    const ClientConfig&     config()      const { return config_; }

    network::NetworkClient& network()           { return *network_; }
    storage::LocalStorage&  storage()           { return *storage_; }
    storage::SessionManager& session()          { return *session_; }
    security::CryptoEngine& crypto()            { return *crypto_; }
    security::TokenManager& tokens()            { return *tokens_; }

    IpcBridge&              ipc()               { return *ipc_; }
    WebViewManager&         webview()           { return *webview_; }
    ClientHttpServer&       http_server()       { return *http_server_; }
    Updater&                updater()           { return *updater_; }
    plugin::PluginManager&  plugin_mgr()        { return *plugin_mgr_; }
    ai::AIProvider&         ai_provider()        { return *ai_provider_; }
    ai::AIChatSession&      ai_session()         { return *ai_session_; }

private:
    AppContext();

    ClientConfig config_;
    std::atomic<bool> running_{false};

    /* 模块 (按初始化顺序) */
    std::unique_ptr<network::NetworkClient> network_;
    std::unique_ptr<storage::LocalStorage>  storage_;
    std::unique_ptr<storage::SessionManager> session_;
    std::unique_ptr<security::CryptoEngine> crypto_;
    std::unique_ptr<security::TokenManager> tokens_;
    std::unique_ptr<IpcBridge>              ipc_;
    std::unique_ptr<WebViewManager>         webview_;
    std::unique_ptr<ClientHttpServer>       http_server_;
    std::unique_ptr<Updater>                updater_;
    std::unique_ptr<plugin::PluginManager>  plugin_mgr_;
    std::unique_ptr<ai::AIProvider>         ai_provider_;
    std::unique_ptr<ai::AIChatSession>      ai_session_;
};

} // namespace app
} // namespace client
} // namespace chrono

#endif // CHRONO_CLIENT_APP_CONTEXT_H
