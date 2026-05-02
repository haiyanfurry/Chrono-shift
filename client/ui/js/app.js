/**
 * Chrono-shift 应用入口
 * 页面路由、事件绑定、全局状态管理
 */

// === 页面切换 ===
function showPage(pageId) {
    $$('.page').forEach(p => p.classList.remove('active'));
    const page = document.getElementById(pageId);
    if (page) page.classList.add('active');
}

// === 显示登录页 ===
function showLogin() {
    $('#form-register').classList.remove('active');
    $('#form-login').classList.add('active');
}

// === 显示注册页 ===
function showRegister() {
    $('#form-login').classList.remove('active');
    $('#form-register').classList.add('active');
}

// === 标签切换（聊天/联系人/社区/群组） ===
function switchTab(tab) {
    // 更新导航按钮状态
    $$('.btn-nav').forEach(b => b.classList.remove('active'));
    const tabBtn = document.getElementById(`tab-${tab}`);
    if (tabBtn) tabBtn.classList.add('active');
    
    if (tab === 'groups') {
        // 群组视图：显示群组列表，隐藏其他列表
        const contactList = $('#contact-list');
        const contactGroups = $('#contact-groups');
        const recentContacts = $('#recent-contacts');
        const groupList = $('#group-list');
        
        if (contactList) contactList.style.display = 'none';
        if (contactGroups) contactGroups.style.display = 'none';
        if (recentContacts) recentContacts.style.display = 'none';
        if (groupList) {
            groupList.style.display = 'block';
            QQGroup.loadGroups();
        }
        
        // 显示群组操作按钮
        const header = $('#chat-header');
        if (header) {
            header.innerHTML = `
                <span class="chat-partner">群组</span>
                <div style="display:flex;gap:8px;">
                    <button class="btn btn-sm btn-primary" onclick="QQGroup.showCreateDialog()">➕ 创建群</button>
                    <button class="btn btn-sm btn-secondary" onclick="QQGroup.showJoinDialog()">🔍 加入群</button>
                </div>
            `;
        }
        
        // 清空聊天区域
        const messagesContainer = $('#chat-messages');
        if (messagesContainer) {
            messagesContainer.innerHTML = '<div class="no-chat-selected"><div class="no-chat-icon">👪</div><p>选择一个群组开始聊天</p></div>';
        }
        $('#chat-input').disabled = true;
        $('#btn-send').disabled = true;
        
        return;
    }
    
    // 非群组视图：恢复联系人列表显示
    const contactList = $('#contact-list');
    const contactGroups = $('#contact-groups');
    const recentContacts = $('#recent-contacts');
    const groupList = $('#group-list');
    
    if (contactList) contactList.style.display = 'block';
    if (contactGroups) contactGroups.style.display = 'block';
    if (recentContacts) recentContacts.style.display = '';
    if (groupList) groupList.style.display = 'none';
    
    // 切换视图
    $$('.content-view').forEach(v => v.classList.remove('active'));
    const view = document.getElementById(`view-${tab}`);
    if (view) view.classList.add('active');
    
    // 加载对应数据
    switch (tab) {
        case 'contacts':
            Contacts.load();
            break;
        case 'community':
            Community.load();
            break;
    }
    
    // 如果切换到聊天但没有选中联系人，恢复默认头部
    if (tab === 'chat' && !Chat.currentPartner) {
        const header = $('#chat-header');
        if (header) {
            header.innerHTML = '<span class="chat-partner">选择一个联系人开始聊天</span>';
        }
    }
}

// === 搜索用户 ===
const onSearchInput = debounce(async function (keyword) {
    if (!keyword.trim()) {
        Contacts.renderSidebar();
        return;
    }
    
    // 使用 Contacts.search 方法（已在 contacts.js 中实现）
    if (window.Contacts && typeof Contacts.search === 'function') {
        Contacts.search(keyword);
    }
}, 500);

// === 消息发送 ===
function sendMessage() {
    Chat.send();
}

function onChatKeydown(event) {
    Chat.onKeydown(event);
}

// === 添加好友 ===
function showAddFriend() {
    // 弹出添加好友对话框
    const overlay = document.createElement('div');
    overlay.className = 'dialog-overlay';
    overlay.innerHTML = `
        <div class="dialog-box dialog-small">
            <div class="dialog-header">
                <h3>👤 添加好友</h3>
                <button class="dialog-close" onclick="this.closest('.dialog-overlay').remove()">&times;</button>
            </div>
            <div class="dialog-body">
                <div class="form-group">
                    <label>对方用户ID 或 用户名</label>
                    <input type="text" id="add-friend-input" placeholder="输入用户ID 搜索">
                </div>
                <div class="form-group">
                    <label>验证消息</label>
                    <input type="text" id="add-friend-message" placeholder="你好，加个好友吧！">
                </div>
                <div id="add-friend-preview" style="margin-top:8px;display:none;"></div>
            </div>
            <div class="dialog-footer">
                <button class="btn btn-secondary" onclick="this.closest('.dialog-overlay').remove()">取消</button>
                <button class="btn btn-primary" onclick="doAddFriend()">发送申请</button>
            </div>
        </div>
    `;
    document.body.appendChild(overlay);
    
    // 搜索预览
    const input = $('#add-friend-input');
    if (input) {
        input.oninput = debounce(async function () {
            const keyword = this.value.trim();
            const preview = $('#add-friend-preview');
            if (!keyword) { preview.style.display = 'none'; return; }
            const result = await API.searchUsers(keyword);
            if (result.status === 'ok' && result.data && result.data.users && result.data.users.length > 0) {
                const user = result.data.users[0];
                preview.innerHTML = `
                    <div class="search-result-item">
                        <img src="${escapeHtml(user.avatar_url || 'assets/images/default_avatar.png')}"
                             style="width:32px;height:32px;border-radius:50%;">
                        <span>${escapeHtml(user.nickname || user.username)}</span>
                    </div>
                `;
                preview.style.display = 'block';
            } else {
                preview.style.display = 'none';
            }
        }, 300);
    }
}

// === 执行添加好友 ===
function doAddFriend() {
    const userId = $('#add-friend-input').value.trim();
    const message = $('#add-friend-message').value.trim() || '你好，加个好友吧！';
    if (!userId) {
        showNotification('请输入用户ID', 'error');
        return;
    }
    if (window.QQFriends && typeof QQFriends.applyFriend === 'function') {
        QQFriends.applyFriend(userId, message);
        document.querySelector('.dialog-overlay')?.remove();
    }
}

// === 上传模板 ===
function showUploadTemplate() {
    Community.showUploadDialog();
}

// === 设置 ===
function showSettings() {
    switchTab('settings');
    
    // 填充当前用户信息
    if (Auth.currentUser) {
        $('#settings-nickname').value = Auth.currentUser.nickname || '';
    }
    
    // 加载 AI 配置到表单
    if (window.AIChat && typeof loadAIConfigToForm === 'function') {
        setTimeout(loadAIConfigToForm, 100);
    }
}

function updateProfile() {
    const nickname = $('#settings-nickname').value.trim();
    if (!nickname) {
        showNotification('昵称不能为空', 'error');
        return;
    }
    
    API.updateProfile(nickname, null).then(result => {
        if (result.status === 'ok') {
            showNotification('资料更新成功', 'success');
            if (Auth.currentUser) {
                Auth.currentUser.nickname = nickname;
            }
        }
    });
}

function resetTheme() {
    ThemeEngine.resetToDefault();
}

function logout() {
    Auth.logout();
}

// === 外部链接跳转 ===
const EXTERNAL_URLS = {
    'bilibili': 'https://www.bilibili.com',
    'acfun': 'https://www.acfun.cn',
    'comic-expo': 'https://www.comic-expo.com'
};

function openExternalUrl(key) {
    const url = EXTERNAL_URLS[key];
    if (!url) {
        showNotification('未知的链接', 'error');
        return;
    }
    
    // 通过 IPC 发送打开 URL 请求（由原生客户端通过系统浏览器打开）
    IPC.send(IPC.MessageType.OPEN_URL, { url: url });
    
    // 在 WebView 中直接通过 window.open 打开（降级方案）
    window.open(url, '_blank');
    
    showNotification(`正在打开: ${url}`, 'info');
}

// === AI 配置管理 ===
function saveAIConfig() {
    if (!window.AIChat) return;
    
    AIChat.config.provider = document.getElementById('ai-provider')?.value || 'openai';
    AIChat.config.apiEndpoint = document.getElementById('ai-endpoint')?.value || '';
    AIChat.config.apiKey = document.getElementById('ai-key')?.value || '';
    AIChat.config.model = document.getElementById('ai-model')?.value || 'gpt-3.5-turbo';
    AIChat.config.maxTokens = parseInt(document.getElementById('ai-max-tokens')?.value || '2048', 10);
    AIChat.config.temperature = parseFloat(document.getElementById('ai-temperature')?.value || '0.7');
    AIChat.config.systemPrompt = document.getElementById('ai-system-prompt')?.value || '你是一个有用的 AI 助手。';
    
    AIChat.saveConfig();
    showNotification('AI 配置已保存', 'success');
}

function testAIConnection() {
    if (!window.AIChat) return;
    
    // 先保存当前表单值到配置
    AIChat.config.apiEndpoint = document.getElementById('ai-endpoint')?.value || '';
    AIChat.config.apiKey = document.getElementById('ai-key')?.value || '';
    
    const statusEl = document.getElementById('ai-connection-status');
    if (statusEl) {
        statusEl.textContent = '⏳ 测试连接中...';
        statusEl.style.color = '#666';
    }
    
    AIChat.testConnection().then(result => {
        if (statusEl) {
            statusEl.textContent = result.success ? '✅ ' + result.message : '❌ ' + result.message;
            statusEl.style.color = result.success ? '#22c55e' : '#ef4444';
        }
    });
}

function onAIProviderChange() {
    const provider = document.getElementById('ai-provider')?.value;
    const endpointInput = document.getElementById('ai-endpoint');
    if (endpointInput && provider === 'openai') {
        endpointInput.placeholder = 'https://api.openai.com';
    } else if (endpointInput) {
        endpointInput.placeholder = 'https://your-custom-api.com';
    }
}

// === 加载 AI 配置到设置表单 ===
function loadAIConfigToForm() {
    if (!window.AIChat) return;
    
    const set = (id, value) => {
        const el = document.getElementById(id);
        if (el) el.value = value;
    };
    
    set('ai-provider', AIChat.config.provider);
    set('ai-endpoint', AIChat.config.apiEndpoint);
    set('ai-key', AIChat.config.apiKey);
    set('ai-model', AIChat.config.model);
    set('ai-max-tokens', AIChat.config.maxTokens);
    set('ai-temperature', AIChat.config.temperature);
    set('ai-system-prompt', AIChat.config.systemPrompt);
    
    const tempVal = document.getElementById('ai-temp-value');
    if (tempVal) tempVal.textContent = AIChat.config.temperature;
}

// === 应用初始化 ===
document.addEventListener('DOMContentLoaded', async function () {
    console.log('Chrono-shift 客户端 v0.1.0 — 墨竹');
    
    // 尝试恢复会话
    if (Auth.restoreSession()) {
        // 已登录，进入主界面
        showPage('page-main');
        
        // 更新用户信息
        const userData = Auth.currentUser;
        if (userData) {
            $('#current-user-name').textContent = userData.nickname || userData.username;
        }
        
        // 加载数据
        Contacts.load();
        
        // 初始化 QQ 社交功能
        if (window.QQFriends && typeof QQFriends.init === 'function') {
            QQFriends.init();
        }
        if (window.QQStatus && typeof QQStatus.init === 'function') {
            QQStatus.init();
        }
        if (window.QQGroup && typeof QQGroup.init === 'function') {
            QQGroup.init();
        }
        
        // 初始化 AI 模块
        if (window.AIChat && typeof AIChat.init === 'function') {
            AIChat.init();
        }
        if (window.AISmartReply && typeof AISmartReply.init === 'function') {
            AISmartReply.init();
        }
    } else {
        // 未登录，显示登录页
        showPage('page-auth');
        showLogin();
    }
    
    // === 登录表单提交 ===
    $('#form-login').addEventListener('submit', async function (e) {
        e.preventDefault();
        
        const username = $('#login-username').value.trim();
        const password = $('#login-password').value;
        
        if (!username || !password) {
            showNotification('请填写用户名和密码', 'error');
            return;
        }
        
        const btn = this.querySelector('.btn');
        btn.disabled = true;
        btn.textContent = '登录中...';
        
        const success = await Auth.login(username, password);
        
        btn.disabled = false;
        btn.textContent = '登录';
        
        if (success) {
            showPage('page-main');
            $('#current-user-name').textContent = Auth.currentUser?.nickname || username;
            Contacts.load();
        }
    });
    
    // === 注册表单提交 ===
    $('#form-register').addEventListener('submit', async function (e) {
        e.preventDefault();
        
        const username = $('#reg-username').value.trim();
        const nickname = $('#reg-nickname').value.trim();
        const password = $('#reg-password').value;
        const confirm = $('#reg-confirm').value;
        
        if (!username || !nickname || !password) {
            showNotification('请填写所有必填字段', 'error');
            return;
        }
        
        if (password !== confirm) {
            showNotification('两次密码不一致', 'error');
            return;
        }
        
        if (password.length < 6) {
            showNotification('密码至少6位', 'error');
            return;
        }
        
        const btn = this.querySelector('.btn');
        btn.disabled = true;
        btn.textContent = '注册中...';
        
        const success = await Auth.register(username, password, nickname);
        
        btn.disabled = false;
        btn.textContent = '注册';
        
        if (success) {
            // 注册即登录，直接进入主界面
            showPage('page-main');
            $('#current-user-name').textContent = Auth.currentUser?.nickname || username;
            Contacts.load();
        }
    });
});
