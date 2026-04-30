/**
 * Chrono-shift C-JS IPC 通信接口
 * 
 * 通过 window.external 与 C#/C++ 宿主通信
 * 此文件封装了前后端通信的细节
 */

const IPC = window.IPC || {};

// === IPC 消息类型 ===
IPC.MessageType = {
    LOGIN:          0x01,
    LOGOUT:         0x02,
    SEND_MESSAGE:   0x10,
    GET_MESSAGES:   0x11,
    GET_CONTACTS:   0x20,
    GET_TEMPLATES:  0x30,
    APPLY_TEMPLATE: 0x31,
    FILE_UPLOAD:    0x40,
    SYSTEM_NOTIFY:  0xFF
};

// === 发送消息到 C 后端 ===
IPC.send = function (type, data) {
    const message = JSON.stringify({
        type: type,
        data: data || {},
        timestamp: Date.now()
    });
    
    // 通过 WebView2 的 host object 发送
    if (window.chrome && window.chrome.webview) {
        window.chrome.webview.postMessage(message);
    } else {
        console.log('[IPC Send]', message);
    }
};

// === 接收来自 C 后端消息 ===
IPC.onMessage = function (callback) {
    // 在 WebView2 中监听来自宿主端的消息
    if (window.chrome && window.chrome.webview) {
        window.chrome.webview.addEventListener('message', (event) => {
            try {
                const msg = JSON.parse(event.data);
                callback(msg);
            } catch (e) {
                console.error('[IPC Parse Error]', e);
            }
        });
    }
};

// === 初始化 IPC ===
IPC.init = function () {
    console.log('[IPC] 初始化完成');
    
    IPC.onMessage(function (msg) {
        console.log('[IPC] 收到消息:', msg);
        // 分发到对应的处理器
        switch (msg.type) {
            case IPC.MessageType.SYSTEM_NOTIFY:
                if (msg.data && msg.data.message) {
                    showNotification(msg.data.message, msg.data.type || 'info');
                }
                break;
            // 其他类型的消息由具体模块处理
        }
    });
};

// IPC 初始化
document.addEventListener('DOMContentLoaded', () => IPC.init());

window.IPC = IPC;
