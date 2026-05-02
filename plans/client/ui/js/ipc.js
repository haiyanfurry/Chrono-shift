/**
 * Chrono-shift C-JS IPC 通信接口
 * 
 * 通过 window.external 与 C#/C++ 宿主通信
 * 此文件封装了前后端通信的细节
 */

const IPC = window.IPC || {};

// === IPC 消息类型 ===
IPC.MessageType = {
    // 系统消息 (0x01-0x6F)
    LOGIN:          0x01,
    LOGOUT:         0x02,
    SEND_MESSAGE:   0x10,
    GET_MESSAGES:   0x11,
    GET_CONTACTS:   0x20,
    GET_TEMPLATES:  0x30,
    APPLY_TEMPLATE: 0x31,
    FILE_UPLOAD:    0x40,
    OPEN_URL:       0x50,
    CONNECTED:      0x60,
    DISCONNECTED:   0x61,

    // 插件系统保留 (0x70-0x9F)
    PLUGIN_BASE:     0x70,

    // 扩展系统保留 (0xA0-0xEF)
    EXTENSION_BASE:  0xA0,

    // AI 系统保留 (0xF0-0xFE)
    AI_BASE:         0xF0,
    AI_CHAT:         0xF0,
    AI_SMART_REPLY:  0xF1,
    AI_TRANSLATE:    0xF2,
    AI_SUMMARIZE:    0xF3,
    AI_IMAGE_GEN:    0xF4,
    AI_TTS:          0xF5,

    // 系统通知
    SYSTEM_NOTIFY:  0xFF
};

// === IPC 类型范围检查 ===
IPC.isPluginType = function (value) {
    return value >= 0x70 && value <= 0x9F;
};
IPC.isExtensionType = function (value) {
    return value >= 0xA0 && value <= 0xEF;
};
IPC.isAIType = function (value) {
    return value >= 0xF0 && value <= 0xFE;
};
IPC.isSystemType = function (value) {
    return value <= 0x6F || value === 0xFF;
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

// === 扩展系统命名空间 (ChronoExtensions) ===
const ChronoExtensions = {
    // 已注册的扩展列表
    _extensions: {},

    /**
     * 注册扩展模块
     * @param {string} id      扩展唯一 ID
     * @param {object} api     扩展 API 对象
     * @param {object} meta    扩展元数据 {name, version, description}
     * @returns {boolean} 是否注册成功
     */
    register: function (id, api, meta) {
        if (this._extensions[id]) {
            console.warn('[ChronoExtensions] 扩展已存在:', id);
            return false;
        }
        this._extensions[id] = {
            id: id,
            api: api || {},
            meta: meta || { name: id, version: '0.1.0' },
            handlers: {},
            panels: []
        };
        console.log('[ChronoExtensions] 扩展已注册:', id, meta);
        return true;
    },

    /**
     * 取消注册扩展模块
     * @param {string} id 扩展 ID
     */
    unregister: function (id) {
        if (this._extensions[id]) {
            delete this._extensions[id];
            console.log('[ChronoExtensions] 扩展已卸载:', id);
        }
    },

    /**
     * 获取已注册的扩展列表
     * @returns {object[]} 扩展列表
     */
    list: function () {
        return Object.keys(this._extensions).map(function (id) {
            return {
                id: id,
                meta: this._extensions[id].meta
            };
        }.bind(this));
    },

    /**
     * 通过 IPC 发送扩展消息
     * @param {number} subType  扩展子类型 (0xA0-0xEF)
     * @param {object} data     消息数据
     */
    send: function (subType, data) {
        IPC.send(subType, data);
    },

    /**
     * 监听扩展消息
     * @param {string}   extId    扩展 ID
     * @param {number}   type     消息类型
     * @param {function} handler  处理回调
     */
    on: function (extId, type, handler) {
        var ext = this._extensions[extId];
        if (!ext) { return; }
        if (!ext.handlers[type]) {
            ext.handlers[type] = [];
        }
        ext.handlers[type].push(handler);
    },

    /**
     * 发送 HTTP 请求到扩展本地服务
     * @param {string} method  HTTP 方法
     * @param {string} path    请求路径
     * @param {object} data    请求数据
     * @returns {Promise}      响应 Promise
     */
    http: function (method, path, data) {
        var url = 'http://127.0.0.1:9010' + path;
        var opts = {
            method: method,
            headers: { 'Content-Type': 'application/json' }
        };
        if (data && method !== 'GET') {
            opts.body = JSON.stringify(data);
        }
        return fetch(url, opts).then(function (r) { return r.json(); });
    },

    /**
     * 创建扩展 UI 面板
     * @param {string} extId      扩展 ID
     * @param {object} config     面板配置 {title, icon, html}
     */
    createPanel: function (extId, config) {
        var ext = this._extensions[extId];
        if (!ext) { return; }
        ext.panels.push(config);
        // 触发面板创建事件
        var event = new CustomEvent('extension-panel-created', {
            detail: { extId: extId, config: config }
        });
        document.dispatchEvent(event);
    }
};

// 暴露到全局
window.ChronoExtensions = ChronoExtensions;

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
        // 分发给扩展和插件的消息处理器
        var extHandled = false;
        if (IPC.isPluginType(msg.type) || IPC.isExtensionType(msg.type) || IPC.isAIType(msg.type)) {
            // 通知所有注册的扩展
            Object.keys(ChronoExtensions._extensions).forEach(function (extId) {
                var ext = ChronoExtensions._extensions[extId];
                if (ext && ext.handlers[msg.type]) {
                    ext.handlers[msg.type].forEach(function (handler) {
                        handler(msg.data, msg);
                    });
                    extHandled = true;
                }
            });
        }

        // 系统消息处理
        switch (msg.type) {
            case IPC.MessageType.SYSTEM_NOTIFY:
                if (msg.data && msg.data.message) {
                    showNotification(msg.data.message, msg.data.type || 'info');
                }
                break;
            case IPC.MessageType.CONNECTED:
                console.log('[IPC] 连接已恢复');
                break;
            case IPC.MessageType.DISCONNECTED:
                console.warn('[IPC] 连接已断开');
                break;
            // 其他类型的消息由具体模块处理
        }

        // 未处理的扩展消息日志
        if (!extHandled && (IPC.isPluginType(msg.type) || IPC.isExtensionType(msg.type) || IPC.isAIType(msg.type))) {
            console.log('[IPC] 未处理的扩展消息 type=0x' + msg.type.toString(16), msg.data);
        }
    });
};

// IPC 初始化
document.addEventListener('DOMContentLoaded', () => IPC.init());

window.IPC = IPC;
