/**
 * Chrono-shift 客户端应用上下文
 * C++17 重构版
 */
#include "AppContext.h"

#include <chrono>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#endif

namespace chrono {
namespace client {
namespace app {

AppContext& AppContext::instance()
{
    static AppContext ctx;
    return ctx;
}

AppContext::AppContext()
    : network_(std::make_unique<network::NetworkClient>())
    , storage_(std::make_unique<storage::LocalStorage>())
    , session_(std::make_unique<storage::SessionManager>())
    , crypto_(std::make_unique<security::CryptoEngine>())
    , tokens_(std::make_unique<security::TokenManager>())
    , ipc_(std::make_unique<IpcBridge>())
    , webview_(std::make_unique<WebViewManager>())
    , http_server_(std::make_unique<ClientHttpServer>())
    , updater_(std::make_unique<Updater>())
    , plugin_mgr_(std::make_unique<plugin::PluginManager>())
    , ai_provider_(nullptr) // 延迟创建，等待配置
    , ai_session_(nullptr)
{
}

AppContext::~AppContext()
{
    stop();
}

int AppContext::init(const ClientConfig& config)
{
    config_ = config;

    /* 设置日志级别 */
    util::Logger::instance().set_level(config.log_level);

    LOG_INFO("Chrono-shift 客户端启动中...");

    /* 1. 初始化存储 */
    if (storage_->init(config.app_data_path) != 0) {
        LOG_WARN("存储初始化失败");
    }

    /* 2. 初始化会话 */
    session_->init();

    /* 3. 初始化加密引擎 */
    if (crypto_->init() != 0) {
        LOG_WARN("加密引擎初始化失败");
    }

    /* 4. 初始化令牌管理器 */
    tokens_->init();

    /* 5. 初始化 IPC 桥接 */
    ipc_->init();

    /* 6. 初始化插件管理器 */
    if (plugin_mgr_->init(config.app_data_path + "/plugins",
                          ipc_.get(), http_server_.get()) != 0) {
        LOG_WARN("插件管理器初始化失败");
    }

    /* 7. 尝试加载 AI 配置 */
    {
        std::string ai_config_json;
        if (storage_->load_config("ai_config", ai_config_json) == 0 && !ai_config_json.empty()) {
            auto ai_config = ai::AIConfig::from_json(ai_config_json);
            if (ai_config.is_valid()) {
                auto provider = ai::AIProvider::create(ai_config.provider_type, ai_config);
                if (provider) {
                    ai_provider_ = std::move(provider);
                    ai_session_ = std::make_unique<ai::AIChatSession>(
                        ai::AIProvider::create(ai_config.provider_type, ai_config));
                    LOG_INFO("AI Provider 已加载: %s", ai_provider_->name().c_str());
                }
            }
        } else {
            LOG_DEBUG("未找到 AI 配置，跳过 AI 初始化");
        }
    }

    LOG_INFO("客户端初始化完成");
    return 0;
}

int AppContext::run()
{
    /* 创建主窗口和 WebView2 */
    if (!webview_->create_window(1200, 800, "Chrono-shift")) {
        LOG_ERROR("创建窗口失败");
        return 1;
    }

    /* 加载前端页面 */
    if (webview_->load_html("ui/index.html") != 0) {
        LOG_ERROR("加载前端页面失败");
        return 1;
    }

    /* 启动本地 HTTP 服务 (端口 9010) */
    if (!http_server_->start(9010)) {
        LOG_WARN("本地HTTP服务启动失败，部分本地功能不可用");
    }

    /* 连接服务器 (TLS 强制) */
    running_ = true;
    LOG_INFO("Chrono-shift 客户端已启动");

    {
        int ret = network_->connect(config_.server_host, config_.server_port);
        if (ret != 0) {
            LOG_WARN("初始连接服务器失败，将在后台自动重连");
        }
    }

    /* 消息循环 (带连接健康检查) */
    constexpr uint32_t kHealthCheckInterval = 15000; /* 每 15 秒 */
    uint64_t last_health_check = 0;

    while (running_) {
        if (!webview_->process_messages()) {
            break;
        }

        /* 周期性连接健康检查 */
        auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();

        if (now_ms - last_health_check >= kHealthCheckInterval) {
            last_health_check = now_ms;

            if (!network_->is_connected()) {
                if (config_.auto_reconnect) {
                    LOG_INFO("检测到连接断开，尝试自动重连...");
                    if (network_->reconnect()) {
                        LOG_INFO("自动重连成功");
                        /* 通知前端连接恢复 */
                        ipc_->send_to_js(webview_->get_webview(),
                                         "{\"status\":\"reconnected\"}");
                    } else {
                        LOG_DEBUG("重连尚未成功，将在下一周期继续尝试");
                    }
                }
            } else {
                LOG_DEBUG("连接健康检查通过");
            }
        }

        /* 保持 UI 响应 */
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    /* 清理 */
    stop();
    return 0;
}

void AppContext::stop()
{
    if (!running_.exchange(false)) {
        return; /* 已经停止 */
    }

    LOG_INFO("客户端正在关闭...");

    plugin_mgr_->stop_all();
    plugin_mgr_->unload_all();
    network_->disconnect();
    http_server_->stop();
    webview_->destroy();

    LOG_INFO("客户端已关闭");
}

} // namespace app
} // namespace client
} // namespace chrono
