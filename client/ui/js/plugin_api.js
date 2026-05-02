/**
 * Chrono-shift 前端插件 API
 *
 * 提供 JS 插件注册、IPC 通信、HTTP 调用、存储和 UI 扩展能力
 */
(function () {
    'use strict';

    /**
     * 插件 API 命名空间
     */
    window.PluginAPI = {
        /** 已注册的插件列表 */
        _plugins: {},

        /** IPC 消息处理器映射 */
        _handlers: {},

        /** HTTP 路由映射 */
        _routes: {},

        /** 插件专用存储 */
        _storage: {},

        // ============================================================
        // 插件生命周期
        // ============================================================

        /**
         * 注册 JS 插件
         * @param {object} manifest - 插件清单
         * @param {object} api - 插件实现
         * @returns {boolean}
         */
        register: function (manifest, api) {
            if (!manifest || !manifest.id) {
                console.error('[PluginAPI] 插件清单缺少 id');
                return false;
            }

            if (this._plugins[manifest.id]) {
                console.warn('[PluginAPI] 插件已注册: ' + manifest.id);
                return false;
            }

            var plugin = {
                manifest: manifest,
                api: api || {},
                state: 'registered',
                created: Date.now()
            };

            this._plugins[manifest.id] = plugin;

            // 注册 IPC 消息处理器
            if (manifest.ipc_types && Array.isArray(manifest.ipc_types)) {
                var self = this;
                manifest.ipc_types.forEach(function (type) {
                    self.on(type, function (data) {
                        if (api.onIpcMessage) {
                            api.onIpcMessage(type, data);
                        }
                    });
                });
            }

            console.log('[PluginAPI] 插件已注册: ' + manifest.id + ' v' + manifest.version);

            // 触发初始化
            if (api.onInit) {
                try {
                    api.onInit();
                } catch (e) {
                    console.error('[PluginAPI] 插件初始化失败: ' + manifest.id, e);
                }
            }

            return true;
        },

        /**
         * 注销插件
         * @param {string} pluginId
         */
        unregister: function (pluginId) {
            var plugin = this._plugins[pluginId];
            if (!plugin) return;

            if (plugin.api.onDestroy) {
                try { plugin.api.onDestroy(); } catch (e) {}
            }

            // 清理消息处理器
            var self = this;
            Object.keys(this._handlers).forEach(function (key) {
                self._handlers[key] = (self._handlers[key] || []).filter(function (h) {
                    return h.pluginId !== pluginId;
                });
            });

            delete this._plugins[pluginId];
            console.log('[PluginAPI] 插件已注销: ' + pluginId);
        },

        // ============================================================
        // IPC 消息
        // ============================================================

        /**
         * 订阅 IPC 消息
         * @param {number} type - 消息类型 (0x70-0xFF)
         * @param {function} handler - 处理器函数(data)
         * @param {string} [pluginId] - 插件 ID
         */
        on: function (type, handler, pluginId) {
            if (!this._handlers[type]) {
                this._handlers[type] = [];
            }
            this._handlers[type].push({
                handler: handler,
                pluginId: pluginId || 'anonymous'
            });
        },

        /**
         * 发送 IPC 消息到 C++ 后端
         * @param {number} type - 消息类型
         * @param {object} data - 消息数据
         */
        send: function (type, data) {
            if (typeof IPC !== 'undefined' && IPC.send) {
                IPC.send(type, data);
            } else {
                console.warn('[PluginAPI] IPC 不可用');
            }
        },

        // ============================================================
        // HTTP 调用
        // ============================================================

        /**
         * 调用本地 HTTP API
         * @param {string} method - GET/POST/PUT/DELETE
         * @param {string} path - API 路径
         * @param {object} [data] - 请求数据
         * @returns {Promise<object>}
         */
        http: function (method, path, data) {
            var url = 'https://127.0.0.1:9010' + path;
            var options = {
                method: method,
                headers: { 'Content-Type': 'application/json' }
            };
            if (data && method !== 'GET') {
                options.body = JSON.stringify(data);
            }
            return fetch(url, options).then(function (res) { return res.json(); });
        },

        // ============================================================
        // 存储
        // ============================================================

        /**
         * 插件专属存储 (基于 localStorage)
         * @param {string} pluginId
         * @param {string} key
         * @param {*} [value] - 不传则读取
         * @returns {*}
         */
        storage: function (pluginId, key, value) {
            var storageKey = 'chrono_plugin_' + pluginId + '_' + key;
            if (value === undefined) {
                var raw = localStorage.getItem(storageKey);
                try { return JSON.parse(raw); } catch (e) { return raw; }
            }
            localStorage.setItem(storageKey, JSON.stringify(value));
            return value;
        },

        // ============================================================
        // UI 扩展
        // ============================================================

        /**
         * 在侧边栏创建面板
         * @param {object} config - { id, title, icon, html, css }
         * @returns {HTMLElement|null}
         */
        createPanel: function (config) {
            if (!config || !config.id) return null;

            var sidebar = document.querySelector('.external-links-list');
            if (!sidebar) return null;

            // 创建侧边栏按钮
            var linkItem = document.createElement('a');
            linkItem.className = 'external-link-item';
            linkItem.href = '#';
            linkItem.id = 'plugin-panel-' + config.id;
            linkItem.textContent = config.title || config.id;
            linkItem.onclick = function () {
                PluginAPI._showPanel(config.id);
            };
            sidebar.appendChild(linkItem);

            // 创建面板内容
            var container = document.createElement('div');
            container.id = 'plugin-view-' + config.id;
            container.className = 'plugin-view content-view';
            container.style.display = 'none';
            if (config.html) {
                container.innerHTML = config.html;
            }

            var mainContent = document.querySelector('.main-content');
            if (mainContent) {
                mainContent.appendChild(container);
            }

            if (config.css) {
                var style = document.createElement('style');
                style.id = 'plugin-style-' + config.id;
                style.textContent = config.css;
                document.head.appendChild(style);
            }

            return container;
        },

        /** @private 显示插件面板 */
        _showPanel: function (pluginId) {
            // 隐藏所有 content-view
            document.querySelectorAll('.content-view').forEach(function (el) {
                el.classList.remove('active');
                el.style.display = 'none';
            });
            // 显示插件面板
            var panel = document.getElementById('plugin-view-' + pluginId);
            if (panel) {
                panel.classList.add('active');
                panel.style.display = 'block';
            }
        }
    };

    // ============================================================
    // IPC 消息分发桥接
    // ============================================================

    /**
     * 被 C++ 后端调用的全局消息分发函数
     * @param {number} type - 消息类型
     * @param {string} jsonData - JSON 字符串
     */
    window.__plugin_dispatch = function (type, jsonData) {
        var data;
        try { data = JSON.parse(jsonData); } catch (e) { data = jsonData; }

        var handlers = PluginAPI._handlers[type] || [];
        handlers.forEach(function (entry) {
            try {
                entry.handler(data);
            } catch (e) {
                console.error('[PluginAPI] 处理器错误:', e);
            }
        });
    };

    console.log('[PluginAPI] 前端插件 API 初始化完成');
})();
