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
    config->server_port = 8080;
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

    /* 连接服务器 */
    g_running = true;
    LOG_INFO("Chrono-shift 客户端已启动");

    /* 消息循环 */
    while (g_running) {
        if (!webview_process_messages(&g_webview)) {
            break;
        }
        /* 保持 UI 响应 */
        Sleep(10);
    }

    /* 清理 */
    client_stop();
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
