#ifndef CHRONO_IPC_BRIDGE_H
#define CHRONO_IPC_BRIDGE_H

#include <stdint.h>
#include <stddef.h>

/* ============================================================
 * C-JS IPC 桥接
 * 实现前端 JavaScript 与 C 后端的双向通信
 * ============================================================ */

/* --- IPC 消息类型 --- */
enum IpcMessageType {
    IPC_LOGIN          = 0x01,
    IPC_LOGOUT         = 0x02,
    IPC_SEND_MESSAGE   = 0x10,
    IPC_GET_MESSAGES   = 0x11,
    IPC_GET_CONTACTS   = 0x20,
    IPC_GET_TEMPLATES  = 0x30,
    IPC_APPLY_TEMPLATE = 0x31,
    IPC_FILE_UPLOAD    = 0x40,
    IPC_OPEN_URL       = 0x50,  /* 打开外部 URL（漫展等） */
    IPC_SYSTEM_NOTIFY  = 0xFF
};

/* --- IPC 回调 --- */
typedef void (*IpcCallback)(enum IpcMessageType type, const char* json_data, void* user_data);

/* --- API --- */
int  ipc_bridge_init(IpcCallback callback, void* user_data);
int  ipc_send_to_js(const char* webview, const char* json_str);
int  ipc_handle_from_js(const char* json_str);
void ipc_register_handler(enum IpcMessageType type, IpcCallback handler);

#endif /* CHRONO_IPC_BRIDGE_H */
