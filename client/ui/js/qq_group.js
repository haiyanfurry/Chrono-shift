/**
 * Chrono-shift QQ 风格群组/讨论组管理
 * 创建群、加入/退出、群聊、成员管理、群公告、讨论组
 */

const QQGroup = window.QQGroup || {};

// === 状态 ===
QQGroup.list = [];                // 我的群组列表
QQGroup.currentGroup = null;      // 当前查看的群组
QQGroup.members = [];             // 当前群组成员
QQGroup.messages = [];            // 当前群聊消息
QQGroup.discussions = [];         // 讨论组列表

// === 加载群组列表 ===
QQGroup.loadGroups = async function () {
    const result = await API.getGroupList();
    if (result.status === 'ok' && result.data) {
        QQGroup.list = result.data.groups || [];
        QQGroup.renderGroupList();
    }
};

/**
 * 创建群组
 */
QQGroup.createGroup = async function (name, introduction, avatar) {
    if (!name || !name.trim()) {
        showNotification('请输入群组名称', 'error');
        return false;
    }
    const result = await API.createGroup({
        name: name.trim(),
        introduction: introduction || '',
        avatar_url: avatar || ''
    });
    if (result.status === 'ok') {
        await QQGroup.loadGroups();
        showNotification('群组创建成功', 'success');
        return true;
    }
    showNotification(result.message || '创建群组失败', 'error');
    return false;
};

/**
 * 加入群组
 */
QQGroup.joinGroup = async function (groupId) {
    const result = await API.joinGroup(groupId);
    if (result.status === 'ok') {
        await QQGroup.loadGroups();
        showNotification('已加入群组', 'success');
        return true;
    }
    showNotification(result.message || '加入失败', 'error');
    return false;
};

/**
 * 退出群组
 */
QQGroup.leaveGroup = async function (groupId) {
    if (!confirm('确定退出该群组吗？')) return false;
    const result = await API.leaveGroup(groupId);
    if (result.status === 'ok') {
        await QQGroup.loadGroups();
        if (QQGroup.currentGroup && QQGroup.currentGroup.id === groupId) {
            QQGroup.currentGroup = null;
        }
        showNotification('已退出群组', 'success');
        return true;
    }
    showNotification(result.message || '退出失败', 'error');
    return false;
};

/**
 * 打开群聊
 */
QQGroup.openChat = async function (groupId, groupName) {
    QQGroup.currentGroup = { id: groupId, name: groupName };
    
    // 更新聊天头部
    const header = $('#chat-header');
    if (header) {
        header.innerHTML = `
            <span class="chat-partner">${escapeHtml(groupName)}</span>
            <span class="chat-partner-status">群聊 (${QQGroup.members.length}人)</span>
            <button class="btn btn-sm btn-secondary" onclick="QQGroup.showGroupDetail()">群详情</button>
        `;
    }
    
    $('#chat-input').disabled = false;
    $('#btn-send').disabled = false;
    $('#chat-input').focus();
    
    // 加载群成员和消息
    await QQGroup.loadMembers(groupId);
    await QQGroup.loadMessages(groupId);
};

/**
 * 加载群成员
 */
QQGroup.loadMembers = async function (groupId) {
    const result = await API.getGroupMembers(groupId);
    if (result.status === 'ok' && result.data) {
        QQGroup.members = result.data.members || [];
    }
};

/**
 * 加载群消息
 */
QQGroup.loadMessages = async function (groupId, offset = 0) {
    const result = await API.getGroupMessages(groupId, offset);
    const container = $('#chat-messages');
    if (!container) return;
    
    if (offset === 0) container.innerHTML = '';
    
    if (result.status === 'ok' && result.data) {
        QQGroup.messages = result.data.messages || [];
        QQGroup.messages.forEach(msg => QQGroup.renderGroupMessage(msg));
    }
    
    container.scrollTop = container.scrollHeight;
};

/**
 * 渲染群消息
 */
QQGroup.renderGroupMessage = function (msg) {
    const container = $('#chat-messages');
    const isSelf = msg.from_id === Auth.currentUser?.user_id;
    
    const messageDiv = document.createElement('div');
    messageDiv.className = `message ${isSelf ? 'message-self' : 'message-other'} fade-in`;
    
    // 群消息显示发送者昵称
    const senderName = msg.from_nickname || msg.from_username || '未知用户';
    
    messageDiv.innerHTML = `
        ${!isSelf ? `<img class="msg-avatar" src="${escapeHtml(msg.avatar_url || 'assets/images/default_avatar.png')}" 
                        onerror="this.src='assets/images/default_avatar.png'">` : ''}
        <div class="message-content">
            ${!isSelf ? `<div class="msg-sender-name">${escapeHtml(senderName)}</div>` : ''}
            <div class="message-bubble">${escapeHtml(msg.content)}</div>
            <div class="message-time">${formatTime(msg.timestamp)}</div>
        </div>
    `;
    
    container.appendChild(messageDiv);
    container.scrollTop = container.scrollHeight;
};

/**
 * 发送群消息
 */
QQGroup.sendMessage = async function () {
    const input = $('#chat-input');
    const content = input.value.trim();
    
    if (!content || !QQGroup.currentGroup) return;
    
    input.value = '';
    
    const result = await API.sendGroupMessage(QQGroup.currentGroup.id, content);
    
    if (result.status === 'ok') {
        QQGroup.renderGroupMessage({
            from_id: Auth.currentUser?.user_id,
            from_nickname: Auth.currentUser?.nickname,
            content: content,
            timestamp: Date.now(),
            avatar_url: Auth.currentUser?.avatar_url
        });
    } else {
        showNotification('消息发送失败', 'error');
    }
};

/**
 * 显示群详情
 */
QQGroup.showGroupDetail = function () {
    if (!QQGroup.currentGroup) return;
    
    const membersHtml = QQGroup.members.map(m => `
        <div class="group-member-item">
            <img src="${escapeHtml(m.avatar_url || 'assets/images/default_avatar.png')}" 
                 onerror="this.src='assets/images/default_avatar.png'">
            <div class="member-info">
                <span class="member-name">${escapeHtml(m.nickname || m.username)}</span>
                ${m.role === 'owner' ? '<span class="member-role owner">群主</span>' : 
                  m.role === 'admin' ? '<span class="member-role admin">管理</span>' : ''}
            </div>
        </div>
    `).join('');
    
    const overlay = document.createElement('div');
    overlay.className = 'dialog-overlay';
    overlay.innerHTML = `
        <div class="dialog-box dialog-large">
            <div class="dialog-header">
                <h3>📋 ${escapeHtml(QQGroup.currentGroup.name)}</h3>
                <button class="dialog-close" onclick="this.closest('.dialog-overlay').remove()">&times;</button>
            </div>
            <div class="dialog-body">
                <div class="group-detail-section">
                    <h4>成员 (${QQGroup.members.length})</h4>
                    <div class="group-members-list">${membersHtml}</div>
                </div>
            </div>
            <div class="dialog-footer">
                <button class="btn btn-secondary" onclick="QQGroup.leaveGroup('${QQGroup.currentGroup.id}'); this.closest('.dialog-overlay').remove()">退出群组</button>
                <button class="btn btn-secondary" onclick="this.closest('.dialog-overlay').remove()">关闭</button>
            </div>
        </div>
    `;
    document.body.appendChild(overlay);
};

/**
 * 显示创建群组弹窗
 */
QQGroup.showCreateDialog = function () {
    const overlay = document.createElement('div');
    overlay.className = 'dialog-overlay';
    overlay.innerHTML = `
        <div class="dialog-box dialog-small">
            <div class="dialog-header">
                <h3>🆕 创建群组</h3>
                <button class="dialog-close" onclick="this.closest('.dialog-overlay').remove()">&times;</button>
            </div>
            <div class="dialog-body">
                <div class="form-group">
                    <label>群组名称</label>
                    <input type="text" id="create-group-name" placeholder="输入群名称">
                </div>
                <div class="form-group">
                    <label>群简介</label>
                    <input type="text" id="create-group-intro" placeholder="选填">
                </div>
            </div>
            <div class="dialog-footer">
                <button class="btn btn-secondary" onclick="this.closest('.dialog-overlay').remove()">取消</button>
                <button class="btn btn-primary" onclick="doCreateGroup()">创建</button>
            </div>
        </div>
    `;
    document.body.appendChild(overlay);
};

/**
 * 显示加入群组弹窗
 */
QQGroup.showJoinDialog = function () {
    const groupId = prompt('请输入群组 ID：');
    if (groupId) {
        QQGroup.joinGroup(groupId.trim());
    }
};

/**
 * 渲染群组列表（侧边栏或视图）
 */
QQGroup.renderGroupList = function () {
    const container = $('#group-list');
    if (!container) return;
    
    container.innerHTML = '';
    
    if (QQGroup.list.length === 0) {
        container.innerHTML = '<div class="loading">暂无群组</div>';
        return;
    }
    
    QQGroup.list.forEach(group => {
        const item = document.createElement('div');
        item.className = 'group-item';
        item.innerHTML = `
            <div class="group-avatar">${escapeHtml(group.name.charAt(0) || '👥')}</div>
            <div class="group-info">
                <div class="group-name">${escapeHtml(group.name)}</div>
                <div class="group-meta">${group.member_count || 0}人</div>
            </div>
        `;
        item.onclick = () => QQGroup.openChat(group.id, group.name);
        container.appendChild(item);
    });
};

// === 讨论组 ===

/**
 * 创建讨论组（临时群聊）
 */
QQGroup.createDiscussion = function (userIds) {
    showNotification('讨论组功能开发中', 'info');
};

// === 初始化 ===
QQGroup.init = function () {
    QQGroup.loadGroups();
};

// 全局函数
function doCreateGroup() {
    const name = $('#create-group-name').value.trim();
    const intro = $('#create-group-intro').value.trim();
    if (name) {
        QQGroup.createGroup(name, intro);
        document.querySelector('.dialog-overlay')?.remove();
    } else {
        showNotification('请输入群组名称', 'error');
    }
}

window.QQGroup = QQGroup;
