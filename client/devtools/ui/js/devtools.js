/**
 * devtools.js — 开发者工具前端模块
 *
 * 提供 8 个图形化调试面板，通过 ChronoExtensions 注册，
 * 并通过 HTTPS API (https://127.0.0.1:9010/api/devtools/*) 与 C++ 后端通信。
 *
 * 面板列表：
 *   1. 命令控制台 (Command Console)
 *   2. 网络监视器 (Network Monitor)
 *   3. 存储查看器 (Storage Inspector)
 *   4. 会话调试 (Session Debug)
 *   5. API 端点测试 (API Endpoint Tester)
 *   6. WebSocket 调试 (WebSocket Debug)
 *   7. 配置编辑器 (Config Editor)
 *   8. 插件检查器 (Plugin Inspector)
 */

(function () {
    'use strict';

    const DEVTOOLS_ID = 'devtools';
    const API_PREFIX = '/api/devtools';

    // ============================================================
    // 工具函数
    // ============================================================

    /** 转义 HTML 防止 XSS */
    function esc(str) {
        var div = document.createElement('div');
        div.textContent = str;
        return div.innerHTML;
    }

    /** 格式化时间戳 */
    function fmtTime(ts) {
        var d = new Date(ts);
        return d.toLocaleTimeString('zh-CN', { hour: '2-digit', minute: '2-digit', second: '2-digit' });
    }

    /** 语法高亮 JSON */
    function highlightJSON(obj) {
        var json = typeof obj === 'string' ? obj : JSON.stringify(obj, null, 2);
        return json.replace(/&/g, '&').replace(/</g, '<').replace(/>/g, '>')
            .replace(/"([^"]+)":/g, '<span class="key">"$1"</span>:')
            .replace(/: "([^"]*)"/g, ': <span class="string">"$1"</span>')
            .replace(/: (\d+\.?\d*)/g, ': <span class="number">$1</span>')
            .replace(/: (true|false)/g, ': <span class="boolean">$1</span>')
            .replace(/: (null)/g, ': <span class="null">$1</span>');
    }

    /** 调用后端 API */
    function api(method, path, data) {
        if (window.ChronoExtensions && ChronoExtensions.http) {
            return ChronoExtensions.http(method, API_PREFIX + path, data);
        }
        // 降级：直接 fetch
        var url = 'https://127.0.0.1:9010' + API_PREFIX + path;
        var opts = {
            method: method,
            headers: { 'Content-Type': 'application/json' }
        };
        if (data && method !== 'GET') {
            opts.body = JSON.stringify(data);
        }
        return fetch(url, opts).then(function (r) { return r.json(); });
    }

    /** GET 请求 */
    function apiGet(path) { return api('GET', path, null); }

    /** POST 请求 */
    function apiPost(path, data) { return api('POST', path, data); }

    // ============================================================
    // DevToolsApp 命名空间
    // ============================================================

    window.DevTools = {
        // 当前激活的面板 ID
        activePanel: 'console',

        // 日志缓冲区 (网络监视器用)
        _logBuffer: [],
        _wsMessages: [],

        // ========================================================
        // 初始化
        // ========================================================

        init: function () {
            console.log('[DevTools] 初始化开发者工具...');

            // 1. 注册扩展
            this.registerExtension();

            // 2. 注入 UI
            this.injectUI();

            // 3. 绑定事件
            this.bindEvents();

            // 4. 启动默认面板
            this.switchPanel('console');

            // 5. 定时刷新状态
            this._statusTimer = setInterval(this.refreshStatus.bind(this), 5000);
            this.refreshStatus();

            console.log('[DevTools] 开发者工具已就绪');
        },

        // ========================================================
        // 扩展注册
        // ========================================================

        registerExtension: function () {
            if (!window.ChronoExtensions) {
                console.warn('[DevTools] ChronoExtensions 不可用，跳过扩展注册');
                return;
            }
            ChronoExtensions.register(DEVTOOLS_ID, {
                getVersion: function () { return '0.1.0'; },
                getPanels: function () { return DevTools.getPanelList(); }
            }, {
                name: '开发者工具',
                version: '0.1.0',
                description: 'Chrono-shift 图形化开发者调试工具'
            });
        },

        getPanelList: function () {
            return [
                { id: 'console',    title: '命令控制台',    icon: '💻' },
                { id: 'network',    title: '网络监视器',    icon: '🌐' },
                { id: 'storage',    title: '存储查看器',    icon: '💾' },
                { id: 'session',    title: '会话调试',      icon: '🔑' },
                { id: 'api-test',   title: 'API 测试',      icon: '🔧' },
                { id: 'websocket',  title: 'WebSocket 调试', icon: '🔌' },
                { id: 'config',     title: '配置编辑器',    icon: '⚙' },
                { id: 'plugins',    title: '插件检查器',    icon: '🧩' }
            ];
        },

        // ========================================================
        // UI 注入
        // ========================================================

        injectUI: function () {
            // --- 1. 在侧边栏底部添加开发者工具按钮 ---
            var sidebarFooter = document.querySelector('.sidebar-footer');
            if (!sidebarFooter) {
                console.warn('[DevTools] .sidebar-footer 未找到');
                return;
            }

            var devtoolsBtn = document.createElement('button');
            devtoolsBtn.className = 'btn-nav';
            devtoolsBtn.id = 'tab-devtools';
            devtoolsBtn.setAttribute('onclick', "switchTab('devtools')");
            devtoolsBtn.innerHTML =
                '<span class="nav-icon">🛠</span>' +
                '<span class="nav-label">调试</span>' +
                '<span class="devtools-notify-dot"></span>';
            sidebarFooter.appendChild(devtoolsBtn);

            // --- 2. 在主内容区添加 devtools 视图 ---
            var mainContent = document.querySelector('.main-content');
            if (!mainContent) {
                console.warn('[DevTools] .main-content 未找到');
                return;
            }

            var view = document.createElement('div');
            view.id = 'view-devtools';
            view.className = 'content-view';
            view.innerHTML = this.buildDevToolsHTML();
            mainContent.appendChild(view);
        },

        /** 生成 devtools 面板 HTML */
        buildDevToolsHTML: function () {
            var panels = this.getPanelList();

            // 标签栏
            var tabsHTML = panels.map(function (p) {
                return '<button class="devtools-tab" data-panel="' + p.id + '" onclick="DevTools.switchPanel(\'' + p.id + '\')">' +
                    esc(p.icon) + ' ' + esc(p.title) + '</button>';
            }).join('');

            // 面板内容
            var panelsHTML = panels.map(function (p) {
                return '<div class="devtools-panel" id="devtools-panel-' + p.id + '" data-panel="' + p.id + '">' +
                    DevTools.getPanelContent(p.id) + '</div>';
            }).join('');

            return '' +
                '<div class="devtools-header">' +
                '  <div style="display:flex;align-items:center;">' +
                '    <h2>🛠 开发者工具</h2>' +
                '    <span class="devtools-badge" id="devtools-status-badge">● 就绪</span>' +
                '  </div>' +
                '  <div class="devtools-header-actions">' +
                '    <button class="devtools-btn small" onclick="DevTools.refreshAll()">🔄 刷新全部</button>' +
                '  </div>' +
                '</div>' +
                '<div class="devtools-tabs">' + tabsHTML + '</div>' +
                '<div class="devtools-panels">' + panelsHTML + '</div>';
        },

        // ========================================================
        // 8 个面板的 HTML 内容
        // ========================================================

        getPanelContent: function (panelId) {
            switch (panelId) {
                case 'console':    return this.panelConsoleHTML();
                case 'network':    return this.panelNetworkHTML();
                case 'storage':    return this.panelStorageHTML();
                case 'session':    return this.panelSessionHTML();
                case 'api-test':   return this.panelApiTestHTML();
                case 'websocket':  return this.panelWebSocketHTML();
                case 'config':     return this.panelConfigHTML();
                case 'plugins':    return this.panelPluginsHTML();
                default:           return '<div class="devtools-empty"><div class="devtools-empty-icon">❓</div><p>未知面板</p></div>';
            }
        },

        // --------------------------------------------------------
        // 面板 1: 命令控制台
        // --------------------------------------------------------
        panelConsoleHTML: function () {
            return '' +
                '<div class="devtools-terminal" style="height:100%;">' +
                '  <div class="devtools-terminal-header">' +
                '    <span class="devtools-terminal-dot red"></span>' +
                '    <span class="devtools-terminal-dot yellow"></span>' +
                '    <span class="devtools-terminal-dot green"></span>' +
                '    <span class="devtools-terminal-title">Chrono-shift CLI Terminal</span>' +
                '  </div>' +
                '  <div class="devtools-terminal-output" id="devtools-console-output">' +
                '    <span class="info">╔══════════════════════════════════════╗</span>\n' +
                '    <span class="info">║    Chrono-shift 开发者命令控制台     ║</span>\n' +
                '    <span class="info">║  输入 help 查看可用命令              ║</span>\n' +
                '    <span class="info">╚══════════════════════════════════════╝</span>\n' +
                '  </div>' +
                '  <div class="devtools-terminal-input-row">' +
                '    <span class="devtools-terminal-prompt">$</span>' +
                '    <input class="devtools-terminal-input" id="devtools-console-input" type="text" ' +
                '           placeholder="输入命令 (如: health, status, help)..." autofocus>' +
                '  </div>' +
                '</div>';
        },

        // --------------------------------------------------------
        // 面板 2: 网络监视器
        // --------------------------------------------------------
        panelNetworkHTML: function () {
            return '' +
                '<div class="devtools-card">' +
                '  <div class="devtools-card-header">' +
                '    <span>🌐 网络请求监视器</span>' +
                '    <div style="display:flex;gap:6px;">' +
                '      <button class="devtools-btn small" onclick="DevTools.clearNetworkLog()">清空</button>' +
                '    </div>' +
                '  </div>' +
                '  <div class="devtools-card-body" style="padding:8px;">' +
                '    <div class="devtools-filter-bar">' +
                '      <input class="devtools-input" id="devtools-network-filter" type="text" ' +
                '             placeholder="筛选路径..." oninput="DevTools.filterNetworkLog()" style="min-width:200px;">' +
                '      <label style="font-size:12px;display:flex;align-items:center;gap:4px;">' +
                '        <input type="checkbox" id="devtools-network-autoscroll" checked> 自动滚动' +
                '      </label>' +
                '    </div>' +
                '    <div id="devtools-network-list" style="max-height:400px;overflow-y:auto;">' +
                '      <div class="devtools-empty"><div class="devtools-empty-icon">🌐</div><p>暂无网络请求记录</p></div>' +
                '    </div>' +
                '  </div>' +
                '</div>' +
                '<div class="devtools-card">' +
                '  <div class="devtools-card-header">' +
                '    <span>📊 网络状态</span>' +
                '  </div>' +
                '  <div class="devtools-card-body" id="devtools-network-status">' +
                '    <span class="devtools-status offline"><span class="devtools-status-dot offline"></span> 等待检测...</span>' +
                '  </div>' +
                '</div>';
        },

        // --------------------------------------------------------
        // 面板 3: 存储查看器
        // --------------------------------------------------------
        panelStorageHTML: function () {
            return '' +
                '<div class="devtools-card">' +
                '  <div class="devtools-card-header">' +
                '    <span>💾 本地存储查看器</span>' +
                '    <button class="devtools-btn small" onclick="DevTools.refreshStorage()">🔄 刷新</button>' +
                '  </div>' +
                '  <div class="devtools-card-body">' +
                '    <div class="devtools-split">' +
                '      <div class="devtools-split-left">' +
                '        <table class="devtools-table">' +
                '          <thead><tr><th style="width:30%;">Key</th><th>Value</th></tr></thead>' +
                '          <tbody id="devtools-storage-tbody"></tbody>' +
                '        </table>' +
                '      </div>' +
                '      <div class="devtools-split-right">' +
                '        <div style="font-size:12px;font-weight:600;margin-bottom:8px;">🔍 查看详情</div>' +
                '        <div class="devtools-json-viewer" id="devtools-storage-detail" style="max-height:500px;">选择左侧条目查看详情</div>' +
                '      </div>' +
                '    </div>' +
                '  </div>' +
                '</div>';
        },

        // --------------------------------------------------------
        // 面板 4: 会话调试
        // --------------------------------------------------------
        panelSessionHTML: function () {
            return '' +
                '<div class="devtools-card">' +
                '  <div class="devtools-card-header">' +
                '    <span>🔑 会话状态</span>' +
                '    <button class="devtools-btn small" onclick="DevTools.refreshSession()">🔄 刷新</button>' +
                '  </div>' +
                '  <div class="devtools-card-body">' +
                '    <div class="devtools-json-viewer" id="devtools-session-json" style="max-height:300px;">加载中...</div>' +
                '  </div>' +
                '</div>' +
                '<div class="devtools-card">' +
                '  <div class="devtools-card-header">' +
                '    <span>🔐 Token 信息</span>' +
                '  </div>' +
                '  <div class="devtools-card-body" id="devtools-token-info">' +
                '    <button class="devtools-btn" onclick="DevTools.decodeToken()">🔍 解码 Token</button>' +
                '  </div>' +
                '</div>' +
                '<div class="devtools-card">' +
                '  <div class="devtools-card-header">' +
                '    <span>⚠️ 会话操作</span>' +
                '  </div>' +
                '  <div class="devtools-card-body">' +
                '    <button class="devtools-btn danger" onclick="DevTools.clearSession()">🗑 清除本地会话</button>' +
                '    <span style="font-size:11px;color:#999;margin-left:8px;">仅清除前端缓存，不会影响服务器登录状态</span>' +
                '  </div>' +
                '</div>';
        },

        // --------------------------------------------------------
        // 面板 5: API 端点测试
        // --------------------------------------------------------
        panelApiTestHTML: function () {
            return '' +
                '<div class="devtools-card">' +
                '  <div class="devtools-card-header">' +
                '    <span>🔧 API 端点测试器</span>' +
                '  </div>' +
                '  <div class="devtools-card-body">' +
                '    <div style="display:flex;gap:8px;margin-bottom:8px;">' +
                '      <select class="devtools-input" id="devtools-apitest-method" style="width:100px;flex-shrink:0;">' +
                '        <option value="GET">GET</option>' +
                '        <option value="POST">POST</option>' +
                '        <option value="PUT">PUT</option>' +
                '        <option value="DELETE">DELETE</option>' +
                '      </select>' +
                '      <input class="devtools-input" id="devtools-apitest-path" type="text" ' +
                '             placeholder="/api/devtools/status" value="/api/devtools/status" style="flex:1;">' +
                '      <button class="devtools-btn primary" onclick="DevTools.runApiTest()">🚀 发送</button>' +
                '    </div>' +
                '    <div style="margin-bottom:8px;">' +
                '      <textarea class="devtools-input" id="devtools-apitest-body" rows="4" ' +
                '                placeholder="请求体 (JSON)..." style="font-family:Consolas,monospace;font-size:12px;"></textarea>' +
                '    </div>' +
                '    <div style="display:flex;gap:8px;align-items:center;margin-bottom:8px;">' +
                '      <span style="font-size:12px;font-weight:600;">响应:</span>' +
                '      <span id="devtools-apitest-status" style="font-size:12px;"></span>' +
                '      <span id="devtools-apitest-duration" style="font-size:11px;color:#999;"></span>' +
                '    </div>' +
                '    <div class="devtools-json-viewer" id="devtools-apitest-response" style="max-height:400px;">等待请求...</div>' +
                '  </div>' +
                '</div>';
        },

        // --------------------------------------------------------
        // 面板 6: WebSocket 调试
        // --------------------------------------------------------
        panelWebSocketHTML: function () {
            return '' +
                '<div class="devtools-card">' +
                '  <div class="devtools-card-header">' +
                '    <span>🔌 WebSocket 调试器</span>' +
                '    <div style="display:flex;gap:6px;">' +
                '      <button class="devtools-btn small" onclick="DevTools.clearWSLog()">清空</button>' +
                '    </div>' +
                '  </div>' +
                '  <div class="devtools-card-body" style="padding:8px;">' +
                '    <div class="devtools-filter-bar">' +
                '      <span style="font-size:12px;font-weight:600;" id="devtools-ws-status">状态: 未知</span>' +
                '      <button class="devtools-btn small primary" onclick="DevTools.refreshWSStatus()">🔄 检测连接</button>' +
                '    </div>' +
                '    <div id="devtools-ws-list" style="max-height:350px;overflow-y:auto;">' +
                '      <div class="devtools-empty"><div class="devtools-empty-icon">🔌</div><p>暂无 WebSocket 消息记录</p></div>' +
                '    </div>' +
                '  </div>' +
                '</div>' +
                '<div class="devtools-card">' +
                '  <div class="devtools-card-header">' +
                '    <span>📤 发送 WebSocket 消息</span>' +
                '  </div>' +
                '  <div class="devtools-card-body">' +
                '    <div style="display:flex;gap:8px;">' +
                '      <input class="devtools-input" id="devtools-ws-send-msg" type="text" ' +
                '             placeholder="JSON 消息内容..." style="flex:1;" ' +
                '             onkeydown="if(event.key===\'Enter\')DevTools.sendWSMessage()">' +
                '      <button class="devtools-btn primary" onclick="DevTools.sendWSMessage()">发送</button>' +
                '    </div>' +
                '  </div>' +
                '</div>';
        },

        // --------------------------------------------------------
        // 面板 7: 配置编辑器
        // --------------------------------------------------------
        panelConfigHTML: function () {
            return '' +
                '<div class="devtools-card">' +
                '  <div class="devtools-card-header">' +
                '    <span>⚙ 开发者配置编辑器</span>' +
                '    <div style="display:flex;gap:6px;">' +
                '      <button class="devtools-btn small" onclick="DevTools.refreshConfig()">🔄 加载</button>' +
                '      <button class="devtools-btn small primary" onclick="DevTools.saveConfig()">💾 保存</button>' +
                '    </div>' +
                '  </div>' +
                '  <div class="devtools-card-body">' +
                '    <div style="margin-bottom:8px;font-size:12px;color:#666;">' +
                '      编辑开发者工具配置 (JSON 格式)，修改后点击保存。' +
                '    </div>' +
                '    <textarea class="devtools-input" id="devtools-config-editor" rows="16" ' +
                '              style="font-family:Consolas,monospace;font-size:12px;line-height:1.5;"></textarea>' +
                '    <div id="devtools-config-status" style="margin-top:6px;font-size:11px;color:#999;"></div>' +
                '  </div>' +
                '</div>';
        },

        // --------------------------------------------------------
        // 面板 8: 插件检查器
        // --------------------------------------------------------
        panelPluginsHTML: function () {
            return '' +
                '<div class="devtools-card">' +
                '  <div class="devtools-card-header">' +
                '    <span>🧩 已注册插件</span>' +
                '    <button class="devtools-btn small" onclick="DevTools.refreshPlugins()">🔄 刷新</button>' +
                '  </div>' +
                '  <div class="devtools-card-body" id="devtools-plugins-list">' +
                '    <div class="devtools-empty"><div class="devtools-empty-icon">🧩</div><p>加载中...</p></div>' +
                '  </div>' +
                '</div>' +
                '<div class="devtools-card">' +
                '  <div class="devtools-card-header">' +
                '    <span>📋 扩展列表 (ChronoExtensions)</span>' +
                '  </div>' +
                '  <div class="devtools-card-body" id="devtools-extensions-list">' +
                '    <div class="devtools-empty"><div class="devtools-empty-icon">📋</div><p>加载中...</p></div>' +
                '  </div>' +
                '</div>' +
                '<div class="devtools-card">' +
                '  <div class="devtools-card-header">' +
                '    <span>📄 Plugin Manifest</span>' +
                '  </div>' +
                '  <div class="devtools-card-body">' +
                '    <div style="display:flex;gap:8px;margin-bottom:8px;">' +
                '      <input class="devtools-input" id="devtools-plugin-path" type="text" ' +
                '             placeholder="插件路径 (如: plugins/example_plugin/manifest.json)" style="flex:1;">' +
                '      <button class="devtools-btn primary" onclick="DevTools.loadPluginManifest()">📂 加载</button>' +
                '    </div>' +
                '    <div class="devtools-json-viewer" id="devtools-plugin-manifest" style="max-height:300px;">等待加载...</div>' +
                '  </div>' +
                '</div>';
        },

        // ========================================================
        // 面板切换
        // ========================================================

        switchPanel: function (panelId) {
            this.activePanel = panelId;

            // 更新标签高亮
            var tabs = document.querySelectorAll('.devtools-tab');
            tabs.forEach(function (t) {
                t.classList.toggle('active', t.getAttribute('data-panel') === panelId);
            });

            // 切换面板显示
            var panels = document.querySelectorAll('.devtools-panel');
            panels.forEach(function (p) {
                p.classList.toggle('active', p.getAttribute('data-panel') === panelId);
            });

            // 延迟聚焦控制台输入
            if (panelId === 'console') {
                setTimeout(function () {
                    var inp = document.getElementById('devtools-console-input');
                    if (inp) inp.focus();
                }, 100);
            }
        },

        // ========================================================
        // 事件绑定
        // ========================================================

        bindEvents: function () {
            var self = this;

            // 命令控制台 - 回车执行
            document.addEventListener('keydown', function (e) {
                if (e.key === 'Enter') {
                    var input = document.getElementById('devtools-console-input');
                    if (input && document.activeElement === input) {
                        self.executeCommand(input.value);
                        input.value = '';
                    }
                }
            });

            // 监听 switchTab 扩展 - 当切换到 devtools 时刷新
            document.addEventListener('switch-tab', function (e) {
                if (e.detail && e.detail.tab === 'devtools') {
                    self.onActivate();
                }
            });
        },

        /** 当 devtools 标签被激活时调用 */
        onActivate: function () {
            this.refreshStatus();
            // 延迟刷新数据密集面板
            var self = this;
            setTimeout(function () { self.refreshStorage(); }, 500);
            setTimeout(function () { self.refreshPlugins(); }, 800);
        },

        // ========================================================
        // 扩展 switchTab 支持
        // ========================================================

        /** 供 switchTab 调用的激活函数 */
        activate: function () {
            var view = document.getElementById('view-devtools');
            if (view) {
                view.classList.add('active');
            }
            // 恢复联系人列表 (兼容非群组逻辑)
            var contactList = document.getElementById('contact-list');
            var contactGroups = document.getElementById('contact-groups');
            var recentContacts = document.getElementById('recent-contacts');
            var groupList = document.getElementById('group-list');
            if (contactList) contactList.style.display = 'block';
            if (contactGroups) contactGroups.style.display = 'block';
            if (recentContacts) recentContacts.style.display = '';
            if (groupList) groupList.style.display = 'none';

            this.onActivate();
        },

        /** 供 switchTab 调用的停用函数 */
        deactivate: function () {
            var view = document.getElementById('view-devtools');
            if (view) {
                view.classList.remove('active');
            }
        },

        // ========================================================
        // 刷新
        // ========================================================

        refreshAll: function () {
            this.refreshStatus();
            this.refreshStorage();
            this.refreshSession();
            this.refreshConfig();
            this.refreshPlugins();
            this.refreshWSStatus();
            showNotification('开发者工具已刷新', 'success');
        },

        refreshStatus: function () {
            var self = this;
            apiGet('/status').then(function (data) {
                var badge = document.getElementById('devtools-status-badge');
                if (badge) {
                    badge.textContent = data.enabled ? '● 开发者模式' : '○ 已关闭';
                    badge.style.background = data.enabled ? '#22c55e' : '#6b7280';
                }
            }).catch(function () {
                var badge = document.getElementById('devtools-status-badge');
                if (badge) {
                    badge.textContent = '● 离线';
                    badge.style.background = '#ef4444';
                }
            });
        },

        // ========================================================
        // 面板 1: 命令控制台逻辑
        // ========================================================

        executeCommand: function (cmdLine) {
            var output = document.getElementById('devtools-console-output');
            if (!output || !cmdLine.trim()) return;

            // 回显命令
            var promptSpan = document.createElement('span');
            promptSpan.className = 'prompt';
            promptSpan.textContent = '$ ' + cmdLine;
            output.appendChild(promptSpan);
            output.appendChild(document.createElement('br'));

            // 滚动到底部
            output.scrollTop = output.scrollHeight;

            // 本地命令处理
            if (cmdLine.trim().toLowerCase() === 'clear') {
                output.innerHTML = '';
                return;
            }

            if (cmdLine.trim().toLowerCase() === 'help') {
                this.appendOutput(output, [
                    '可用命令 (通过后端 CLI 引擎执行):',
                    '  health         - 服务器健康检查',
                    '  status         - 开发者模式状态',
                    '  config [key]   - 查看/设置配置',
                    '  session        - 查看会话',
                    '  token [jwt]    - 解码 JWT Token',
                    '  ping           - Ping 服务器',
                    '  network        - 网络状态',
                    '  storage        - 存储状态',
                    '  plugins        - 插件列表',
                    '  connect        - 连接服务器',
                    '  disconnect     - 断开连接',
                    '  clear          - 清屏',
                    '  help           - 本帮助',
                    '',
                    '也支持完整 CLI 命令，如: endpoint test /api/health'
                ].join('\n'), 'info');
                return;
            }

            // 发送到后端执行
            var self = this;
            apiPost('/exec', { cmd: cmdLine }).then(function (data) {
                if (data.status === 'ok' && data.output) {
                    self.appendOutput(output, data.output, 'success');
                } else if (data.message) {
                    self.appendOutput(output, data.message, 'error');
                } else {
                    self.appendOutput(output, '命令执行完成 (无输出)', 'info');
                }
            }).catch(function (err) {
                self.appendOutput(output, '错误: ' + (err.message || '请求失败'), 'error');
            });
        },

        appendOutput: function (outputEl, text, className) {
            var span = document.createElement('span');
            span.className = className || '';
            span.textContent = text;
            outputEl.appendChild(span);
            outputEl.appendChild(document.createElement('br'));
            outputEl.scrollTop = outputEl.scrollHeight;
        },

        // ========================================================
        // 面板 2: 网络监视器逻辑
        // ========================================================

        addNetworkLog: function (method, path, statusCode, body, duration) {
            this._logBuffer.push({
                time: Date.now(),
                method: method,
                path: path,
                status: statusCode,
                body: body,
                duration: duration
            });

            // 限制缓冲区大小
            if (this._logBuffer.length > 500) {
                this._logBuffer.shift();
            }

            this.renderNetworkLog();
        },

        clearNetworkLog: function () {
            this._logBuffer = [];
            this.renderNetworkLog();
        },

        filterNetworkLog: function () {
            this.renderNetworkLog();
        },

        renderNetworkLog: function () {
            var container = document.getElementById('devtools-network-list');
            if (!container) return;

            var filter = document.getElementById('devtools-network-filter');
            var keyword = filter ? filter.value.trim().toLowerCase() : '';
            var autoScroll = document.getElementById('devtools-network-autoscroll');
            var shouldAutoScroll = autoScroll ? autoScroll.checked : true;

            var filtered = this._logBuffer;
            if (keyword) {
                filtered = filtered.filter(function (entry) {
                    return entry.path.toLowerCase().indexOf(keyword) >= 0 ||
                           entry.method.toLowerCase().indexOf(keyword) >= 0;
                });
            }

            if (filtered.length === 0) {
                container.innerHTML = '<div class="devtools-empty"><div class="devtools-empty-icon">🌐</div><p>暂无网络请求记录</p></div>';
                return;
            }

            var html = filtered.slice().reverse().map(function (entry) {
                var statusClass = entry.status < 300 ? 'success' : entry.status < 500 ? 'redirect' : 'error';
                var methodClass = entry.method.toLowerCase();
                var bodyPreview = typeof entry.body === 'string' ?
                    entry.body.substring(0, 200) : JSON.stringify(entry.body || '').substring(0, 200);
                return '' +
                    '<div class="devtools-http-entry">' +
                    '  <div class="devtools-http-summary" onclick="DevTools.toggleHttpDetail(this)">' +
                    '    <span class="devtools-http-method ' + methodClass + '">' + esc(entry.method) + '</span>' +
                    '    <span class="devtools-http-status ' + statusClass + '">' + esc(entry.status) + '</span>' +
                    '    <span class="devtools-http-path">' + esc(entry.path) + '</span>' +
                    '    <span style="font-size:10px;color:#999;">' + esc(entry.duration || '') + '</span>' +
                    '  </div>' +
                    '  <div class="devtools-http-detail">' + esc(bodyPreview) + '</div>' +
                    '</div>';
            }).join('');

            container.innerHTML = html;

            if (shouldAutoScroll) {
                container.scrollTop = 0;
            }
        },

        toggleHttpDetail: function (summaryEl) {
            var detail = summaryEl.nextElementSibling;
            if (detail) {
                detail.classList.toggle('open');
            }
        },

        // ========================================================
        // 面板 3: 存储查看器逻辑
        // ========================================================

        refreshStorage: function () {
            var tbody = document.getElementById('devtools-storage-tbody');
            if (!tbody) return;

            tbody.innerHTML = '<tr><td colspan="2" style="text-align:center;padding:20px;"><span class="devtools-spinner"></span> 加载中...</td></tr>';

            var self = this;
            apiGet('/storage/list').then(function (data) {
                tbody.innerHTML = '';
                if (data.status === 'ok' && data.entries) {
                    var entries = data.entries;
                    if (entries.length === 0) {
                        tbody.innerHTML = '<tr><td colspan="2" style="text-align:center;padding:20px;color:#999;">暂无存储数据</td></tr>';
                        return;
                    }
                    entries.forEach(function (entry) {
                        var tr = document.createElement('tr');
                        tr.style.cursor = 'pointer';
                        tr.innerHTML = '<td style="font-family:Consolas,monospace;font-size:11px;">' + esc(entry.key) + '</td>' +
                                       '<td style="font-family:Consolas,monospace;font-size:11px;max-width:300px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;">' + esc(entry.value || '') + '</td>';
                        tr.onclick = function () {
                            var detail = document.getElementById('devtools-storage-detail');
                            if (detail) {
                                detail.innerHTML = highlightJSON(entry);
                            }
                        };
                        tbody.appendChild(tr);
                    });
                } else {
                    tbody.innerHTML = '<tr><td colspan="2" style="text-align:center;padding:20px;color:#ef4444;">加载失败: ' + esc(data.message || '未知错误') + '</td></tr>';
                }
            }).catch(function (err) {
                tbody.innerHTML = '<tr><td colspan="2" style="text-align:center;padding:20px;color:#ef4444;">请求失败: ' + esc(err.message) + '</td></tr>';
            });
        },

        // ========================================================
        // 面板 4: 会话调试逻辑
        // ========================================================

        refreshSession: function () {
            var jsonView = document.getElementById('devtools-session-json');
            if (!jsonView) return;
            jsonView.innerHTML = '<span class="devtools-spinner"></span> 加载中...';

            apiGet('/status').then(function (data) {
                jsonView.innerHTML = highlightJSON(data);
            }).catch(function (err) {
                jsonView.innerHTML = '<span class="error">请求失败: ' + esc(err.message) + '</span>';
            });

            // 同时尝试获取网络状态
            apiGet('/network/status').then(function (data) {
                var el = document.getElementById('devtools-network-status');
                if (el) {
                    var connected = data.status === 'ok' && data.connected;
                    el.innerHTML = '<span class="devtools-status ' + (connected ? 'online' : 'offline') + '">' +
                        '<span class="devtools-status-dot ' + (connected ? 'online' : 'offline') + '"></span> ' +
                        '服务器: ' + (connected ? '已连接' : '未连接') +
                        '</span>';
                }
            }).catch(function () {
                var el = document.getElementById('devtools-network-status');
                if (el) {
                    el.innerHTML = '<span class="devtools-status offline"><span class="devtools-status-dot offline"></span> 服务器: 无法连接</span>';
                }
            });
        },

        decodeToken: function () {
            var infoEl = document.getElementById('devtools-token-info');
            if (!infoEl) return;

            // 从 sessionStorage 获取 token
            var userData = sessionStorage.getItem('chrono_user');
            if (!userData) {
                infoEl.innerHTML = '<span style="color:#ef4444;font-size:12px;">未登录，无 Token 可解码</span>';
                return;
            }

            try {
                var user = JSON.parse(userData);
                // 尝试从 IPC 获取 token
                apiGet('/session').then(function (data) {
                    if (data.status === 'ok' && data.token) {
                        var parts = data.token.split('.');
                        if (parts.length === 3) {
                            var payload = JSON.parse(atob(parts[1]));
                            infoEl.innerHTML = '<div class="devtools-json-viewer">' + highlightJSON(payload) + '</div>';
                        } else {
                            infoEl.innerHTML = '<span style="font-size:12px;">Token: ' + esc(data.token.substring(0, 40)) + '...</span>';
                        }
                    } else {
                        infoEl.innerHTML = '<span style="font-size:12px;color:#999;">无法获取 Token (可能需要先登录)</span>';
                    }
                }).catch(function () {
                    infoEl.innerHTML = '<span style="font-size:12px;color:#999;">无法连接到后端</span>';
                });
            } catch (e) {
                infoEl.innerHTML = '<span style="color:#ef4444;font-size:12px;">解析会话数据失败</span>';
            }
        },

        clearSession: function () {
            if (!confirm('确定要清除本地会话缓存吗？')) return;
            sessionStorage.removeItem('chrono_user');
            localStorage.removeItem('chrono_user');
            showNotification('本地会话已清除', 'success');
            this.refreshSession();
        },

        // ========================================================
        // 面板 5: API 端点测试逻辑
        // ========================================================

        runApiTest: function () {
            var method = document.getElementById('devtools-apitest-method');
            var path = document.getElementById('devtools-apitest-path');
            var bodyEl = document.getElementById('devtools-apitest-body');
            var statusEl = document.getElementById('devtools-apitest-status');
            var durationEl = document.getElementById('devtools-apitest-duration');
            var responseEl = document.getElementById('devtools-apitest-response');

            if (!method || !path || !responseEl) return;

            var m = method.value;
            var p = path.value.trim() || '/api/devtools/status';
            var body = bodyEl ? bodyEl.value.trim() : '';

            if (statusEl) statusEl.textContent = '⏳ 请求中...';
            if (durationEl) durationEl.textContent = '';
            responseEl.innerHTML = '<span class="devtools-spinner"></span> 发送请求...';

            var startTime = performance.now();

            var fetchOpts = {
                method: m,
                headers: { 'Content-Type': 'application/json' }
            };

            if (body && m !== 'GET') {
                fetchOpts.body = body;
            }

            var url = 'https://127.0.0.1:9010' + p;

            fetch(url, fetchOpts)
                .then(function (resp) {
                    var elapsed = (performance.now() - startTime).toFixed(1);
                    if (durationEl) durationEl.textContent = elapsed + 'ms';
                    if (statusEl) {
                        statusEl.textContent = resp.status + ' ' + resp.statusText;
                        statusEl.style.color = resp.ok ? '#22c55e' : '#ef4444';
                    }

                    // 添加到此网络日志
                    DevTools.addNetworkLog(m, p, resp.status, '', elapsed + 'ms');

                    return resp.text();
                })
                .then(function (text) {
                    try {
                        var parsed = JSON.parse(text);
                        responseEl.innerHTML = highlightJSON(parsed);
                    } catch (e) {
                        responseEl.innerHTML = esc(text);
                    }
                })
                .catch(function (err) {
                    if (statusEl) {
                        statusEl.textContent = '❌ ' + (err.message || '请求失败');
                        statusEl.style.color = '#ef4444';
                    }
                    responseEl.innerHTML = '<span class="error">错误: ' + esc(err.message) + '</span>';
                });
        },

        // ========================================================
        // 面板 6: WebSocket 调试逻辑
        // ========================================================

        refreshWSStatus: function () {
            var statusEl = document.getElementById('devtools-ws-status');
            if (!statusEl) return;

            statusEl.innerHTML = '<span class="devtools-spinner"></span> 检测中...';

            var self = this;
            apiGet('/ws/status').then(function (data) {
                if (data.status === 'ok') {
                    var connected = data.connected;
                    statusEl.innerHTML = '状态: <span class="devtools-status ' + (connected ? 'online' : 'offline') + '">' +
                        '<span class="devtools-status-dot ' + (connected ? 'online' : 'offline') + '"></span> ' +
                        (connected ? '已连接' : '未连接') +
                        '</span>';
                    if (data.url) {
                        statusEl.innerHTML += ' <span style="font-size:11px;color:#999;">(' + esc(data.url) + ')</span>';
                    }
                } else {
                    statusEl.textContent = '状态: 未知 (' + (data.message || '无数据') + ')';
                }
            }).catch(function () {
                statusEl.textContent = '状态: 无法连接后端';
            });
        },

        clearWSLog: function () {
            this._wsMessages = [];
            var container = document.getElementById('devtools-ws-list');
            if (container) {
                container.innerHTML = '<div class="devtools-empty"><div class="devtools-empty-icon">🔌</div><p>暂无 WebSocket 消息记录</p></div>';
            }
        },

        sendWSMessage: function () {
            var input = document.getElementById('devtools-ws-send-msg');
            if (!input || !input.value.trim()) return;

            var msg = input.value.trim();
            var self = this;

            apiPost('/ws/send', { message: msg }).then(function (data) {
                if (data.status === 'ok') {
                    showNotification('WebSocket 消息已发送', 'success');
                    // 记录到日志
                    self._wsMessages.push({
                        time: Date.now(),
                        direction: 'send',
                        content: msg
                    });
                    self.renderWSLog();
                    input.value = '';
                } else {
                    showNotification('发送失败: ' + (data.message || '未知错误'), 'error');
                }
            }).catch(function (err) {
                showNotification('发送失败: ' + (err.message || '请求错误'), 'error');
            });
        },

        addWSMessage: function (direction, content) {
            this._wsMessages.push({
                time: Date.now(),
                direction: direction,
                content: content
            });
            if (this._wsMessages.length > 200) {
                this._wsMessages.shift();
            }
            this.renderWSLog();
        },

        renderWSLog: function () {
            var container = document.getElementById('devtools-ws-list');
            if (!container) return;

            if (this._wsMessages.length === 0) {
                container.innerHTML = '<div class="devtools-empty"><div class="devtools-empty-icon">🔌</div><p>暂无 WebSocket 消息记录</p></div>';
                return;
            }

            var html = this._wsMessages.map(function (entry) {
                var icon = entry.direction === 'send' ? '📤' : '📥';
                var color = entry.direction === 'send' ? '#89b4fa' : '#a6e3a1';
                return '' +
                    '<div class="devtools-log-entry">' +
                    '  <span class="devtools-log-time">' + fmtTime(entry.time) + '</span>' +
                    '  <span style="color:' + color + ';">' + icon + '</span>' +
                    '  <span class="devtools-log-msg" style="font-size:11px;">' + esc(entry.content || '') + '</span>' +
                    '</div>';
            }).reverse().join('');

            container.innerHTML = html;
        },

        // ========================================================
        // 面板 7: 配置编辑器逻辑
        // ========================================================

        refreshConfig: function () {
            var editor = document.getElementById('devtools-config-editor');
            var status = document.getElementById('devtools-config-status');
            if (!editor) return;

            editor.value = '// 加载中...';
            if (status) status.textContent = '加载配置...';

            apiGet('/config').then(function (data) {
                if (data.status === 'ok') {
                    editor.value = JSON.stringify(data.config || data, null, 2);
                    if (status) status.textContent = '✅ 配置已加载';
                } else {
                    // 使用默认配置结构
                    editor.value = JSON.stringify({
                        dev_mode: false,
                        log_level: 'info',
                        max_log_entries: 500,
                        auto_refresh: true,
                        refresh_interval_ms: 5000,
                        theme: 'dark'
                    }, null, 2);
                    if (status) status.textContent = '使用默认配置 (无法从后端获取)';
                }
            }).catch(function () {
                editor.value = JSON.stringify({
                    dev_mode: false,
                    log_level: 'info',
                    max_log_entries: 500,
                    auto_refresh: true,
                    refresh_interval_ms: 5000,
                    theme: 'dark'
                }, null, 2);
                var st = document.getElementById('devtools-config-status');
                if (st) st.textContent = '⚠ 后端不可用，显示默认配置';
            });
        },

        saveConfig: function () {
            var editor = document.getElementById('devtools-config-editor');
            var status = document.getElementById('devtools-config-status');
            if (!editor) return;

            var configStr = editor.value.trim();
            try {
                var config = JSON.parse(configStr);
                apiPost('/config/save', { config: config }).then(function (data) {
                    if (status) {
                        status.textContent = data.status === 'ok' ? '✅ 配置已保存' : '❌ 保存失败: ' + (data.message || '');
                    }
                    if (data.status === 'ok') {
                        showNotification('配置已保存', 'success');
                    }
                }).catch(function () {
                    if (status) status.textContent = '❌ 保存请求失败';
                });
            } catch (e) {
                if (status) status.textContent = '❌ JSON 格式错误: ' + e.message;
            }
        },

        // ========================================================
        // 面板 8: 插件检查器逻辑
        // ========================================================

        refreshPlugins: function () {
            var listEl = document.getElementById('devtools-plugins-list');
            var extEl = document.getElementById('devtools-extensions-list');
            if (!listEl) return;

            listEl.innerHTML = '<div style="text-align:center;padding:20px;"><span class="devtools-spinner"></span> 加载插件列表...</div>';

            var self = this;

            // 获取后端插件列表
            apiGet('/plugins').then(function (data) {
                if (data.status === 'ok' && data.plugins) {
                    if (data.plugins.length === 0) {
                        listEl.innerHTML = '<div class="devtools-empty"><div class="devtools-empty-icon">🧩</div><p>暂无已注册插件</p></div>';
                    } else {
                        var html = '<table class="devtools-table">' +
                            '<thead><tr><th>名称</th><th>版本</th><th>ID</th><th>状态</th></tr></thead><tbody>';
                        data.plugins.forEach(function (p) {
                            var statusClass = p.loaded ? 'green' : 'gray';
                            var statusText = p.loaded ? '已加载' : '未加载';
                            html += '<tr>' +
                                '<td>' + esc(p.name || p.id || '未知') + '</td>' +
                                '<td>' + esc(p.version || '-') + '</td>' +
                                '<td style="font-family:Consolas,monospace;font-size:11px;">' + esc(p.id || '-') + '</td>' +
                                '<td><span class="devtools-badge ' + statusClass + '">' + statusText + '</span></td>' +
                                '</tr>';
                        });
                        html += '</tbody></table>';
                        listEl.innerHTML = html;
                    }
                } else {
                    listEl.innerHTML = '<div class="devtools-empty"><div class="devtools-empty-icon">🧩</div><p>暂无插件数据</p></div>';
                }
            }).catch(function () {
                listEl.innerHTML = '<div class="devtools-empty"><div class="devtools-empty-icon">🧩</div><p>无法获取插件列表 (后端可能未启动)</p></div>';
            });

            // 获取前端扩展列表
            if (extEl) {
                if (window.ChronoExtensions && ChronoExtensions._extensions) {
                    var extIds = Object.keys(ChronoExtensions._extensions);
                    if (extIds.length === 0) {
                        extEl.innerHTML = '<div class="devtools-empty"><div class="devtools-empty-icon">📋</div><p>暂无已注册扩展</p></div>';
                    } else {
                        var extHTML = '<table class="devtools-table">' +
                            '<thead><tr><th>ID</th><th>名称</th><th>版本</th><th>面板数</th></tr></thead><tbody>';
                        extIds.forEach(function (id) {
                            var ext = ChronoExtensions._extensions[id];
                            extHTML += '<tr>' +
                                '<td style="font-family:Consolas,monospace;font-size:11px;">' + esc(id) + '</td>' +
                                '<td>' + esc(ext.meta.name || id) + '</td>' +
                                '<td>' + esc(ext.meta.version || '-') + '</td>' +
                                '<td>' + (ext.panels ? ext.panels.length : 0) + '</td>' +
                                '</tr>';
                        });
                        extHTML += '</tbody></table>';
                        extEl.innerHTML = extHTML;
                    }
                } else {
                    extEl.innerHTML = '<div class="devtools-empty"><div class="devtools-empty-icon">📋</div><p>ChronoExtensions 未初始化</p></div>';
                }
            }
        },

        loadPluginManifest: function () {
            var pathInput = document.getElementById('devtools-plugin-path');
            var manifestEl = document.getElementById('devtools-plugin-manifest');
            if (!pathInput || !manifestEl) return;

            var pluginPath = pathInput.value.trim();
            if (!pluginPath) {
                manifestEl.innerHTML = '<span style="color:#ef4444;">请输入插件路径</span>';
                return;
            }

            manifestEl.innerHTML = '<span class="devtools-spinner"></span> 加载中...';

            // 通过 HTTP API 加载 manifest
            apiPost('/plugins/manifest', { path: pluginPath }).then(function (data) {
                if (data.status === 'ok' && data.manifest) {
                    manifestEl.innerHTML = highlightJSON(data.manifest);
                } else {
                    manifestEl.innerHTML = '<span style="color:#ef4444;">加载失败: ' + esc(data.message || '未找到 manifest') + '</span>';
                }
            }).catch(function () {
                manifestEl.innerHTML = '<span style="color:#ef4444;">请求失败，后端可能未运行</span>';
            });
        }
    };

    // ============================================================
    // 自动初始化
    // ============================================================

    // 在 DOM 加载完成后初始化
    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', function () {
            DevTools.init();
        });
    } else {
        DevTools.init();
    }

})();
