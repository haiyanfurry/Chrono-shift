/**
 * Chrono-shift C-JS IPC 桥接 (骨架)
 * 语言标准: C99
 */

#include "ipc_bridge.h"
#include "client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_IPC_HANDLERS 32

static struct {
    enum IpcMessageType type;
    IpcCallback handler;
} s_handlers[MAX_IPC_HANDLERS];
static size_t s_handler_count = 0;
static IpcCallback s_default_callback = NULL;
static void* s_user_data = NULL;

int ipc_bridge_init(IpcCallback callback, void* user_data)
{
    s_default_callback = callback;
    s_user_data = user_data;
    s_handler_count = 0;
    LOG_INFO("IPC 桥接初始化完成");
    return 0;
}

int ipc_send_to_js(const char* webview, const char* json_str)
{
    (void)webview;
    (void)json_str;
    LOG_DEBUG("发送到 JS: %s", json_str);
    /* Phase 2 实现通过 WebView2 执行 JS */
    return 0;
}

int ipc_handle_from_js(const char* json_str)
{
    if (!json_str) return -1;
    
    LOG_DEBUG("收到 JS 消息: %s", json_str);

    /* 查找注册的处理器 */
    for (size_t i = 0; i < s_handler_count; i++) {
        if (s_handlers[i].handler) {
            s_handlers[i].handler(s_handlers[i].type, json_str, s_user_data);
            return 0;
        }
    }

    /* 默认处理器 */
    if (s_default_callback) {
        s_default_callback(0, json_str, s_user_data);
    }

    return 0;
}

void ipc_register_handler(enum IpcMessageType type, IpcCallback handler)
{
    if (s_handler_count >= MAX_IPC_HANDLERS) {
        LOG_ERROR("IPC 处理器注册表已满");
        return;
    }
    s_handlers[s_handler_count].type = type;
    s_handlers[s_handler_count].handler = handler;
    s_handler_count++;
    LOG_DEBUG("IPC 处理器已注册: type=%d", type);
}
