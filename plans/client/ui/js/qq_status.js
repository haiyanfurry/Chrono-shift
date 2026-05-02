/**
 * Chrono-shift QQ 风格在线状态与签名管理
 * 在线/离线/忙碌/隐身、个性签名、自定义头像、个人名片
 */

const QQStatus = window.QQStatus || {};

// === 状态常量 ===
QQStatus.STATES = {
    ONLINE: { key: 'online', label: '在线', icon: '🟢' },
    OFFLINE: { key: 'offline', label: '离线', icon: '⚪' },
    BUSY: { key: 'busy', label: '忙碌', icon: '🔴' },
    INVISIBLE: { key: 'invisible', label: '隐身', icon: '⚫' }
};

QQStatus.currentStatus = 'online';    // 当前状态
QQStatus.signature = '';             // 个性签名

// === 更新在线状态 ===
QQStatus.updateStatus = async function (statusKey) {
    if (!statusKey || !QQStatus.STATES[statusKey.toUpperCase()]) {
        showNotification('无效的状态', 'error');
        return false;
    }
    
    const result = await API.updateStatus(statusKey, QQStatus.signature);
    if (result.status === 'ok') {
        QQStatus.currentStatus = statusKey;
        QQStatus.updateStatusUI();
        showNotification(`状态已设为: ${QQStatus.STATES[statusKey.toUpperCase()].label}`, 'success');
        return true;
    }
    showNotification(result.message || '状态更新失败', 'error');
    return false;
};

// === 更新个性签名 ===
QQStatus.updateSignature = async function (signature) {
    if (signature && signature.length > 100) {
        showNotification('签名不超过100字', 'error');
        return false;
    }
    
    const result = await API.updateStatus(QQStatus.currentStatus, signature || '');
    if (result.status === 'ok') {
        QQStatus.signature = signature || '';
        showNotification('签名已更新', 'success');
        QQStatus.updateSignatureUI();
        return true;
    }
    showNotification(result.message || '签名更新失败', 'error');
    return false;
};

// === 上传头像 ===
QQStatus.uploadAvatar = async function (file) {
    if (!file) return false;
    
    if (file.size > 5 * 1024 * 1024) {
        showNotification('头像文件不能超过 5MB', 'error');
        return false;
    }
    
    const formData = new FormData();
    formData.append('avatar', file);
    
    const result = await API.uploadAvatar(formData);
    if (result.status === 'ok' && result.data) {
        const avatarUrl = result.data.url;
        
        // 更新头像显示
        const avatarImgs = document.querySelectorAll('.user-avatar img, .contact-avatar-wrap img');
        avatarImgs.forEach(img => {
            if (img.closest('.sidebar-header') || img.closest('#current-user-avatar')) {
                img.src = avatarUrl;
            }
        });
        
        // 更新当前用户信息
        if (Auth.currentUser) {
            Auth.currentUser.avatar_url = avatarUrl;
        }
        
        showNotification('头像已更新', 'success');
        return true;
    }
    showNotification(result.message || '头像上传失败', 'error');
    return false;
};

// === 更新状态显示 UI ===
QQStatus.updateStatusUI = function () {
    const state = QQStatus.STATES[QQStatus.currentStatus.toUpperCase()];
    if (!state) return;
    
    // 更新状态圆点
    const dots = document.querySelectorAll('.status-dot');
    dots.forEach(dot => {
        dot.className = `status-dot status-${QQStatus.currentStatus}`;
    });
    
    // 更新侧边栏状态文字
    const statusLabel = $('#current-user-status');
    if (statusLabel) {
        statusLabel.textContent = `${state.icon} ${state.label}`;
    }
};

// === 更新签名 UI ===
QQStatus.updateSignatureUI = function () {
    const sigEl = $('#current-user-signature');
    if (sigEl) {
        sigEl.textContent = QQStatus.signature || '设置个性签名...';
        sigEl.className = QQStatus.signature ? 'user-signature' : 'user-signature empty';
    }
};

// === 显示状态选择器 ===
QQStatus.showStatusSelector = function () {
    const overlay = document.createElement('div');
    overlay.className = 'dialog-overlay';
    overlay.innerHTML = `
        <div class="dialog-box dialog-small">
            <div class="dialog-header">
                <h3>🟢 设置在线状态</h3>
                <button class="dialog-close" onclick="this.closest('.dialog-overlay').remove()">&times;</button>
            </div>
            <div class="dialog-body">
                <div class="status-selector">
                    ${Object.values(QQStatus.STATES).map(s => `
                        <div class="status-option ${QQStatus.currentStatus === s.key ? 'active' : ''}" 
                             onclick="QQStatus.updateStatus('${s.key}'); this.closest('.dialog-overlay').remove()">
                            <span class="status-icon">${s.icon}</span>
                            <span class="status-label">${s.label}</span>
                        </div>
                    `).join('')}
                </div>
            </div>
        </div>
    `;
    document.body.appendChild(overlay);
};

// === 显示签名编辑 ===
QQStatus.showSignatureEditor = function () {
    const overlay = document.createElement('div');
    overlay.className = 'dialog-overlay';
    overlay.innerHTML = `
        <div class="dialog-box dialog-small">
            <div class="dialog-header">
                <h3>✏️ 编辑个性签名</h3>
                <button class="dialog-close" onclick="this.closest('.dialog-overlay').remove()">&times;</button>
            </div>
            <div class="dialog-body">
                <div class="form-group">
                    <textarea id="signature-input" rows="3" maxlength="100" 
                              placeholder="写下你的个性签名..." 
                              style="width:100%;padding:8px 12px;border:1px solid var(--color-border);border-radius:var(--border-radius-md);resize:none;">${escapeHtml(QQStatus.signature)}</textarea>
                    <div style="text-align:right;font-size:var(--font-size-xs);color:var(--color-text-tertiary);margin-top:4px;">
                        <span id="signature-char-count">${QQStatus.signature.length}</span>/100
                    </div>
                </div>
            </div>
            <div class="dialog-footer">
                <button class="btn btn-secondary" onclick="this.closest('.dialog-overlay').remove()">取消</button>
                <button class="btn btn-primary" onclick="doUpdateSignature()">保存</button>
            </div>
        </div>
    `;
    document.body.appendChild(overlay);
    
    // 字数统计
    const input = $('#signature-input');
    if (input) {
        input.oninput = function () {
            const count = $('#signature-char-count');
            if (count) count.textContent = this.value.length;
        };
    }
};

// === 显示头像上传 ===
QQStatus.showAvatarUploader = function () {
    const input = document.createElement('input');
    input.type = 'file';
    input.accept = 'image/png,image/jpeg,image/gif,image/webp';
    input.onchange = () => {
        if (input.files.length > 0) {
            QQStatus.uploadAvatar(input.files[0]);
        }
    };
    input.click();
};

// === 在侧边栏添加状态和签名 UI ===
QQStatus.injectSidebarUI = function () {
    const header = $('.sidebar-header');
    if (!header) return;
    
    // 添加状态标签
    const statusEl = document.createElement('div');
    statusEl.id = 'current-user-status';
    statusEl.className = 'user-status';
    statusEl.textContent = '🟢 在线';
    statusEl.onclick = () => QQStatus.showStatusSelector();
    header.appendChild(statusEl);
    
    // 添加签名
    const sigEl = document.createElement('div');
    sigEl.id = 'current-user-signature';
    sigEl.className = 'user-signature empty';
    sigEl.textContent = '设置个性签名...';
    sigEl.onclick = () => QQStatus.showSignatureEditor();
    header.appendChild(sigEl);
    
    // 头像点击上传
    const avatar = $('#current-user-avatar');
    if (avatar) {
        avatar.style.cursor = 'pointer';
        avatar.title = '点击更换头像';
        avatar.onclick = () => QQStatus.showAvatarUploader();
    }
};

// === 初始化 ===
QQStatus.init = function () {
    QQStatus.injectSidebarUI();
    QQStatus.updateStatusUI();
};

// 全局函数
function doUpdateSignature() {
    const signature = $('#signature-input').value.trim();
    QQStatus.updateSignature(signature);
    document.querySelector('.dialog-overlay')?.remove();
}

window.QQStatus = QQStatus;
