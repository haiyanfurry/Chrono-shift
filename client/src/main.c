/**
 * Chrono-shift 客户端入口
 * 语言标准: C99
 * 平台: Windows 10/11 x64 (Win32 API + WebView2)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "client.h"
#include "webview_manager.h"
#include "ipc_bridge.h"
#include "network.h"
#include "local_storage.h"
#include "updater.h"
#include "client_http_server.h"

/* 全局上下文 */
static ClientConfig g_config;
static ClientState g_state;
static WebViewContext g_webview;
static NetworkContext g_network;
static StorageContext g_storage;
static bool g_running = false;

/* 设置默认配置 */
static void set_default_config(ClientConfig* config)
{
    memset(config, 0, sizeof(ClientConfig));
    strncpy(config->server_host, "127.0.0.1", sizeof(config->server_host) - 1);
    config->server_port = 4443;
    strncpy(config->app_data_path, "./data", sizeof(config->app_data_path) - 1);
    config->log_level = LOG_INFO;
    config->auto_reconnect = true;
}

/* IPC 回调：处理来自 JS 前端的消息 */
static void on_ipc_message(enum IpcMessageType type, const char* json_data, void* user_data)
{
    (void)user_data;
    LOG_DEBUG("IPC 消息: type=%d, data=%s", type, json_data);
    /* Phase 3+ 实现具体消息分发 */
}

/* WinMain 入口 */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, 
                   LPSTR lpCmdLine, int nCmdShow)
{
    (void)hInstance;
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;

    /* 初始化配置 */
    set_default_config(&g_config);
    memset(&g_state, 0, sizeof(ClientState));

    LOG_INFO("Chrono-shift 客户端启动中...");

    /* 初始化各模块 */
    if (client_init(&g_config) != 0) {
        LOG_ERROR("客户端初始化失败");
        return 1;
    }

    /* 创建主窗口和 WebView2 */
    if (webview_init(&g_webview) != 0) {
        LOG_ERROR("WebView2 初始化失败");
        return 1;
    }

    if (webview_create_window(&g_webview, 1200, 800, "Chrono-shift") != 0) {
        LOG_ERROR("创建窗口失败");
        return 1;
    }

    /* 加载前端页面 */
    if (webview_load_html(&g_webview, "ui/index.html") != 0) {
        LOG_ERROR("加载前端页面失败");
        return 1;
    }

    /* 初始化 IPC 桥接 */
    ipc_bridge_init(on_ipc_message, NULL);

    /* 启动本地 HTTP 服务 (端口 9010) */
    if (client_http_server_start(9010) != 0) {
        LOG_WARN("本地HTTP服务启动失败，部分本地功能不可用");
    }

    /* 连接服务器 (TLS 强制) */
    net_set_tls(&g_network, true);
    g_running = true;
    LOG_INFO("Chrono-shift 客户端已启动");

    /* 初始连接到服务器 */
    {
        int ret = net_connect(&g_network, g_config.server_host, g_config.server_port);
        if (ret != 0) {
            LOG_WARN("初始连接服务器失败，将在后台自动重连");
        }
    }

    /* 消息循环 (带连接健康检查) */
    uint32_t last_health_check = GetTickCount();
    uint32_t health_check_interval = 15000; /* 每 15 秒检查一次 */

    while (g_running) {
        if (!webview_process_messages(&g_webview)) {
            break;
        }

        /* 周期性连接健康检查 */
        uint32_t now = GetTickCount();
        if (now - last_health_check >= health_check_interval) {
            last_health_check = now;

            if (!net_is_connected(&g_network)) {
                if (g_config.auto_reconnect) {
                    LOG_INFO("检测到连接断开，尝试自动重连...");
                    if (net_reconnect(&g_network) == 0) {
                        LOG_INFO("自动重连成功");
                        /* 通知前端连接恢复 */
                        ipc_send_to_js(g_webview.webview, "{\"status\":\"reconnected\"}");
                    } else {
                        LOG_DEBUG("重连尚未成功，将在下一周期继续尝试");
                    }
                }
            } else {
                /* 连接正常，可选的心跳保活 */
                LOG_DEBUG("连接健康检查通过");
            }
        }

        /* 保持 UI 响应 */
        Sleep(50); /* 略微提高响应性 */
    }

    /* 清理 */
    client_stop();
    client_http_server_stop();
    webview_destroy(&g_webview);
    LOG_INFO("客户端已关闭");

    return 0;
}

int client_init(const ClientConfig* config)
{
    memcpy(&g_config, config, sizeof(ClientConfig));
    
    /* 初始化存储 */
    if (storage_init(&g_storage, config->app_data_path) != 0) {
        LOG_WARN("存储初始化失败");
    }

    /* 初始化网络 */
    net_init(&g_network);

    LOG_INFO("客户端初始化完成");
    return 0;
}

int client_start(void)
{
    LOG_INFO("客户端开始运行");
    return 0;
}

void client_stop(void)
{
    g_running = false;
    net_disconnect(&g_network);
    LOG_INFO("客户端已停止");
}

void client_get_state(ClientState* state)
{
    if (state) {
        memcpy(state, &g_state, sizeof(ClientState));
    }
}
