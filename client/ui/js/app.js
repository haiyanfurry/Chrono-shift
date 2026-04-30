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

// === 标签切换（聊天/联系人/社区） ===
function switchTab(tab) {
    // 更新导航按钮状态
    $$('.btn-nav').forEach(b => b.classList.remove('active'));
    const tabBtn = document.getElementById(`tab-${tab}`);
    if (tabBtn) tabBtn.classList.add('active');
    
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
}

// === 搜索用户 ===
const onSearchInput = debounce(async function (keyword) {
    if (!keyword.trim()) {
        Contacts.renderSidebar();
        return;
    }
    
    const result = await API.searchUsers(keyword);
    if (result.status === 'ok' && result.data) {
        const container = $('#contact-list');
        container.innerHTML = '';
        
        (result.data.users || []).forEach(user => {
            const item = document.createElement('div');
            item.className = 'contact-item';
            item.innerHTML = `
                <div class="user-avatar">
                    <img src="${user.avatar_url || 'assets/images/default_avatar.png'}" alt="头像">
                </div>
                <div class="contact-info">
                    <div class="contact-name">${escapeHtml(user.nickname || user.username)}</div>
                    <div class="contact-preview">点击添加好友</div>
                </div>
            `;
            item.onclick = () => {
                IPC.send(IPC.MessageType.SYSTEM_NOTIFY, {
                    message: `添加好友功能开发中 - Phase 4`,
                    type: 'info'
                });
            };
            container.appendChild(item);
        });
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
    showNotification('添加好友功能开发中 - Phase 4', 'info');
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

// === 应用初始化 ===
document.addEventListener('DOMContentLoaded', async function () {
    console.log('Chrono-shift 客户端 v0.1.0');
    
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
            showLogin();
        }
    });
});
