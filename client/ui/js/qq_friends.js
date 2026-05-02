/**
 * Chrono-shift QQ 风格好友管理
 * 好友分组、备注、验证、黑名单、最近联系人
 */

const QQFriends = window.QQFriends || {};

// === 状态 ===
QQFriends.groups = [];             // 好友分组 [{ id, name, count }]
QQFriends.recentContacts = [];     // 最近联系人
QQFriends.pendingRequests = [];    // 待处理的好友请求
QQFriends.blacklist = [];          // 黑名单
QQFriends.currentGroup = null;     // 当前选中的分组

// === 分组颜色 ===
QQFriends.GROUP_COLORS = [
    '#12B7F5', '#1AAD19', '#F5A623', '#E54545',
    '#9B59B6', '#2ECC71', '#E67E22', '#1ABC9C'
];

// === 好友分组 API ===

/**
 * 加载好友分组
 */
QQFriends.loadGroups = async function () {
    const result = await API.getFriendGroups();
    if (result.status === 'ok' && result.data) {
        QQFriends.groups = result.data.groups || [];
        QQFriends.renderGroups();
    } else {
        // 如果后端不支持分组，使用默认分组
        QQFriends.groups = [
            { id: 'default', name: '我的好友', count: 0, color: '#12B7F5' }
        ];
    }
};

/**
 * 创建好友分组
 */
QQFriends.createGroup = async function (name) {
    if (!name || !name.trim()) {
        showNotification('请输入分组名称', 'error');
        return false;
    }
    const result = await API.createFriendGroup({ name: name.trim() });
    if (result.status === 'ok') {
        await QQFriends.loadGroups();
        showNotification('分组创建成功', 'success');
        return true;
    }
    showNotification(result.message || '创建分组失败', 'error');
    return false;
};

/**
 * 重命名分组
 */
QQFriends.renameGroup = async function (groupId, newName) {
    if (!newName || !newName.trim()) return false;
    const result = await API.renameFriendGroup(groupId, newName.trim());
    if (result.status === 'ok') {
        await QQFriends.loadGroups();
        return true;
    }
    return false;
};

/**
 * 删除分组
 */
QQFriends.deleteGroup = async function (groupId) {
    const result = await API.deleteFriendGroup(groupId);
    if (result.status === 'ok') {
        await QQFriends.loadGroups();
        showNotification('分组已删除', 'success');
        return true;
    }
    return false;
};

/**
 * 将好友移到分组
 */
QQFriends.moveToGroup = async function (userId, groupId) {
    const result = await API.moveFriendToGroup(userId, groupId);
    if (result.status === 'ok') {
        showNotification('已移动至分组', 'success');
        await QQFriends.loadGroups();
        return true;
    }
    return false;
};

// === 好友备注 ===

/**
 * 设置好友备注
 */
QQFriends.setFriendNote = async function (userId, note) {
    const result = await API.updateFriendNote(userId, note);
    if (result.status === 'ok') {
        // 更新本地数据
        const contact = Contacts.list.find(c => c.user_id === userId);
        if (contact) {
            contact.note = note;
            Contacts.renderSidebar();
        }
        showNotification('备注已更新', 'success');
        return true;
    }
    showNotification(result.message || '设置备注失败', 'error');
    return false;
};

// === 好友验证 ===

/**
 * 加载待处理的好友请求
 */
QQFriends.loadPendingRequests = async function () {
    const result = await API.getFriendRequests();
    if (result.status === 'ok' && result.data) {
        QQFriends.pendingRequests = result.data.requests || [];
        QQFriends.renderPendingRequests();
    }
};

/**
 * 发送好友申请
 */
QQFriends.applyFriend = async function (userId, message) {
    const result = await API.addFriendRequest(userId, {
        message: message || '你好，加个好友吧！'
    });
    if (result.status === 'ok') {
        showNotification('好友申请已发送', 'success');
        return true;
    }
    showNotification(result.message || '发送申请失败', 'error');
    return false;
};

/**
 * 同意好友申请
 */
QQFriends.approveRequest = async function (requestId) {
    const result = await API.approveFriendRequest(requestId);
    if (result.status === 'ok') {
        await QQFriends.loadPendingRequests();
        // 刷新联系人列表
        if (window.Contacts && typeof Contacts.load === 'function') {
            Contacts.load();
        }
        showNotification('已同意好友申请', 'success');
        return true;
    }
    showNotification(result.message || '操作失败', 'error');
    return false;
};

/**
 * 拒绝好友申请
 */
QQFriends.rejectRequest = async function (requestId) {
    const result = await API.rejectFriendRequest(requestId);
    if (result.status === 'ok') {
        await QQFriends.loadPendingRequests();
        return true;
    }
    return false;
};

// === 黑名单 ===

/**
 * 加载黑名单
 */
QQFriends.loadBlacklist = async function () {
    const result = await API.getBlacklist();
    if (result.status === 'ok' && result.data) {
        QQFriends.blacklist = result.data.users || [];
        QQFriends.renderBlacklist();
    }
};

/**
 * 拉黑用户
 */
QQFriends.blockUser = async function (userId) {
    if (!confirm('确定要将该用户加入黑名单吗？')) return false;
    const result = await API.blockUser(userId);
    if (result.status === 'ok') {
        await QQFriends.loadBlacklist();
        showNotification('已加入黑名单', 'success');
        return true;
    }
    showNotification(result.message || '操作失败', 'error');
    return false;
};

/**
 * 取消拉黑
 */
QQFriends.unblockUser = async function (userId) {
    const result = await API.unblockUser(userId);
    if (result.status === 'ok') {
        await QQFriends.loadBlacklist();
        showNotification('已移出黑名单', 'success');
        return true;
    }
    return false;
};

// === 最近联系人 ===

/**
 * 加载最近联系人
 */
QQFriends.loadRecentContacts = async function () {
    const result = await API.getRecentContacts();
    if (result.status === 'ok' && result.data) {
        QQFriends.recentContacts = result.data.contacts || [];
        QQFriends.renderRecentContacts();
    }
};

// === 渲染 UI ===

/**
 * 渲染分组列表（侧边栏）
 */
QQFriends.renderGroups = function () {
    const container = $('#contact-groups');
    if (!container) return;

    container.innerHTML = '';

    // 全部联系人
    const allItem = document.createElement('div');
    allItem.className = `contact-group-item ${QQFriends.currentGroup === null ? 'active' : ''}`;
    allItem.innerHTML = `
        <span class="group-icon">👥</span>
        <span class="group-name">全部联系人</span>
        <span class="group-count">${Contacts.list.length}</span>
    `;
    allItem.onclick = () => {
        QQFriends.currentGroup = null;
        QQFriends.renderGroups();
        Contacts.renderSidebar();
    };
    container.appendChild(allItem);

    // 最近联系人
    const recentItem = document.createElement('div');
    recentItem.className = `contact-group-item ${QQFriends.currentGroup === 'recent' ? 'active' : ''}`;
    recentItem.innerHTML = `
        <span class="group-icon">🕐</span>
        <span class="group-name">最近联系人</span>
        <span class="group-count">${QQFriends.recentContacts.length}</span>
    `;
    recentItem.onclick = () => {
        QQFriends.currentGroup = 'recent';
        QQFriends.renderGroups();
        QQFriends.showRecentContacts();
    };
    container.appendChild(recentItem);

    // 好友请求
    if (QQFriends.pendingRequests.length > 0) {
        const pendingItem = document.createElement('div');
        pendingItem.className = 'contact-group-item has-badge';
        pendingItem.innerHTML = `
            <span class="group-icon">📩</span>
            <span class="group-name">好友请求</span>
            <span class="group-badge">${QQFriends.pendingRequests.length}</span>
        `;
        pendingItem.onclick = () => QQFriends.showPendingRequestsDialog();
        container.appendChild(pendingItem);
    }

    // 用户自定义分组
    QQFriends.groups.forEach(group => {
        const item = document.createElement('div');
        item.className = `contact-group-item ${QQFriends.currentGroup === group.id ? 'active' : ''}`;
        const color = group.color || QQFriends.GROUP_COLORS[Math.floor(Math.random() * QQFriends.GROUP_COLORS.length)];
        item.innerHTML = `
            <span class="group-dot" style="background:${color}"></span>
            <span class="group-name">${escapeHtml(group.name)}</span>
            <span class="group-count">${group.count || 0}</span>
        `;
        item.onclick = () => {
            QQFriends.currentGroup = group.id;
            QQFriends.renderGroups();
            QQFriends.filterByGroup(group.id);
        };
        // 右键菜单
        item.oncontextmenu = (e) => {
            e.preventDefault();
            QQFriends.showGroupContextMenu(e, group);
        };
        container.appendChild(item);
    });

    // 添加分组按钮
    const addGroupItem = document.createElement('div');
    addGroupItem.className = 'contact-group-item group-add-btn';
    addGroupItem.innerHTML = '<span class="group-icon">➕</span><span class="group-name">添加分组</span>';
    addGroupItem.onclick = () => QQFriends.showCreateGroupDialog();
    container.appendChild(addGroupItem);

    // 黑名单入口
    if (QQFriends.blacklist.length > 0) {
        const blockItem = document.createElement('div');
        blockItem.className = 'contact-group-item';
        blockItem.innerHTML = `
            <span class="group-icon">🚫</span>
            <span class="group-name">黑名单</span>
            <span class="group-count">${QQFriends.blacklist.length}</span>
        `;
        blockItem.onclick = () => QQFriends.showBlacklistDialog();
        container.appendChild(blockItem);
    }
};

/**
 * 按分组过滤联系人
 */
QQFriends.filterByGroup = function (groupId) {
    const container = $('#contact-list');
    if (!container) return;

    container.innerHTML = '';

    const filtered = Contacts.list.filter(c => c.group_id === groupId);

    if (filtered.length === 0) {
        container.innerHTML = '<div class="loading">该分组暂无联系人</div>';
        return;
    }

    filtered.forEach(contact => {
        const item = QQFriends.createContactItem(contact);
        container.appendChild(item);
    });
};

/**
 * 显示最近联系人
 */
QQFriends.showRecentContacts = function () {
    const container = $('#contact-list');
    if (!container) return;

    container.innerHTML = '';

    if (QQFriends.recentContacts.length === 0) {
        container.innerHTML = '<div class="loading">暂无最近联系人</div>';
        return;
    }

    QQFriends.recentContacts.forEach(contact => {
        const item = QQFriends.createContactItem(contact);
        container.appendChild(item);
    });
};

/**
 * 创建联系人列表项（QQ 风格）
 */
QQFriends.createContactItem = function (contact) {
    const item = document.createElement('div');
    item.className = 'contact-item qq-contact-item';
    item.dataset.id = contact.user_id;

    const displayName = contact.note || contact.nickname || contact.username || '未知用户';

    // 在线状态圆点
    const statusDot = document.createElement('span');
    statusDot.className = `contact-status-dot ${contact.online ? 'online' : 'offline'}`;

    item.innerHTML = `
        <div class="contact-avatar-wrap">
            <img src="${escapeHtml(contact.avatar_url || 'assets/images/default_avatar.png')}" alt="头像"
                 onerror="this.src='assets/images/default_avatar.png'">
            <span class="contact-status-dot ${contact.online ? 'online' : 'offline'}"></span>
        </div>
        <div class="contact-info">
            <div class="contact-name-row">
                <span class="contact-name">${escapeHtml(displayName)}</span>
                ${contact.note ? `<span class="contact-note-badge">备注</span>` : ''}
            </div>
            <div class="contact-preview">${escapeHtml(contact.last_message || contact.signature || '')}</div>
        </div>
        <div class="contact-time">${formatTime(contact.last_time)}</div>
    `;

    // 左键点击打开聊天
    item.onclick = () => {
        if (window.Chat && typeof Chat.open === 'function') {
            Chat.open(contact.user_id, displayName);
        }
    };

    // 右键菜单
    item.oncontextmenu = (e) => {
        e.preventDefault();
        QQFriends.showContactContextMenu(e, contact);
    };

    return item;
};

/**
 * 渲染最近联系人到独立区域
 */
QQFriends.renderRecentContacts = function () {
    const container = $('#recent-contacts');
    if (!container) return;

    container.innerHTML = '';

    if (QQFriends.recentContacts.length === 0) {
        container.style.display = 'none';
        return;
    }

    container.style.display = 'block';

    const title = document.createElement('div');
    title.className = 'recent-title';
    title.textContent = '最近联系人';
    container.appendChild(title);

    QQFriends.recentContacts.slice(0, 5).forEach(contact => {
        const item = document.createElement('div');
        item.className = 'recent-contact-item';
        const displayName = contact.note || contact.nickname || contact.username || '未知用户';
        item.innerHTML = `
            <img src="${escapeHtml(contact.avatar_url || 'assets/images/default_avatar.png')}" alt="头像"
                 onerror="this.src='assets/images/default_avatar.png'">
            <span>${escapeHtml(displayName)}</span>
        `;
        item.onclick = () => {
            if (window.Chat && typeof Chat.open === 'function') {
                Chat.open(contact.user_id, displayName);
            }
        };
        container.appendChild(item);
    });
};

/**
 * 渲染待处理的好友请求弹窗
 */
QQFriends.renderPendingRequests = function () {
    const container = $('#pending-requests-list');
    if (!container) return;
    container.innerHTML = '';

    if (QQFriends.pendingRequests.length === 0) {
        container.innerHTML = '<div class="loading">暂无待处理的请求</div>';
        return;
    }

    QQFriends.pendingRequests.forEach(req => {
        const item = document.createElement('div');
        item.className = 'friend-request-item';
        item.innerHTML = `
            <img src="${escapeHtml(req.avatar_url || 'assets/images/default_avatar.png')}" alt="头像"
                 onerror="this.src='assets/images/default_avatar.png'">
            <div class="request-info">
                <div class="request-name">${escapeHtml(req.nickname || req.username)}</div>
                <div class="request-message">${escapeHtml(req.message || '请求添加好友')}</div>
            </div>
            <div class="request-actions">
                <button class="btn btn-sm btn-primary" onclick="QQFriends.approveRequest('${req.id}')">同意</button>
                <button class="btn btn-sm btn-secondary" onclick="QQFriends.rejectRequest('${req.id}')">拒绝</button>
            </div>
        `;
        container.appendChild(item);
    });
};

/**
 * 渲染黑名单
 */
QQFriends.renderBlacklist = function () {
    const container = $('#blacklist-list');
    if (!container) return;
    container.innerHTML = '';

    if (QQFriends.blacklist.length === 0) {
        container.innerHTML = '<div class="loading">黑名单为空</div>';
        return;
    }

    QQFriends.blacklist.forEach(user => {
        const item = document.createElement('div');
        item.className = 'blacklist-item';
        item.innerHTML = `
            <img src="${escapeHtml(user.avatar_url || 'assets/images/default_avatar.png')}" alt="头像"
                 onerror="this.src='assets/images/default_avatar.png'">
            <div class="blacklist-info">
                <div class="blacklist-name">${escapeHtml(user.nickname || user.username)}</div>
            </div>
            <button class="btn btn-sm btn-secondary" onclick="QQFriends.unblockUser('${user.user_id}')">移出黑名单</button>
        `;
        container.appendChild(item);
    });
};

// === 弹窗管理 ===

/**
 * 显示创建分组弹窗
 */
QQFriends.showCreateGroupDialog = function () {
    const name = prompt('请输入分组名称：');
    if (name) {
        QQFriends.createGroup(name);
    }
};

/**
 * 显示好友请求弹窗
 */
QQFriends.showPendingRequestsDialog = function () {
    // 创建弹窗
    const overlay = document.createElement('div');
    overlay.className = 'dialog-overlay';
    overlay.innerHTML = `
        <div class="dialog-box dialog-medium">
            <div class="dialog-header">
                <h3>📩 好友请求</h3>
                <button class="dialog-close" onclick="this.closest('.dialog-overlay').remove()">&times;</button>
            </div>
            <div class="dialog-body" id="pending-requests-list">
                <!-- 动态渲染 -->
            </div>
            <div class="dialog-footer">
                <button class="btn btn-secondary" onclick="this.closest('.dialog-overlay').remove()">关闭</button>
            </div>
        </div>
    `;
    document.body.appendChild(overlay);
    QQFriends.renderPendingRequests();
};

/**
 * 显示黑名单弹窗
 */
QQFriends.showBlacklistDialog = function () {
    const overlay = document.createElement('div');
    overlay.className = 'dialog-overlay';
    overlay.innerHTML = `
        <div class="dialog-box dialog-medium">
            <div class="dialog-header">
                <h3>🚫 黑名单</h3>
                <button class="dialog-close" onclick="this.closest('.dialog-overlay').remove()">&times;</button>
            </div>
            <div class="dialog-body" id="blacklist-list">
                <!-- 动态渲染 -->
            </div>
            <div class="dialog-footer">
                <button class="btn btn-secondary" onclick="this.closest('.dialog-overlay').remove()">关闭</button>
            </div>
        </div>
    `;
    document.body.appendChild(overlay);
    QQFriends.renderBlacklist();
};

/**
 * 显示好友右键菜单
 */
QQFriends.showContactContextMenu = function (event, contact) {
    QQFriends.removeContextMenu();

    const menu = document.createElement('div');
    menu.className = 'context-menu';
    menu.style.left = event.clientX + 'px';
    menu.style.top = event.clientY + 'px';

    const displayName = contact.note || contact.nickname || contact.username;

    menu.innerHTML = `
        <div class="context-menu-header">${escapeHtml(displayName)}</div>
        <div class="context-menu-item" onclick="QQFriends.showSetNoteDialog('${contact.user_id}', '${escapeHtml(displayName)}')">
            ✏️ 设置备注
        </div>
        <div class="context-menu-item" onclick="QQFriends.showMoveGroupDialog('${contact.user_id}', '${escapeHtml(displayName)}')">
            📂 移动至分组
        </div>
        <div class="context-menu-divider"></div>
        <div class="context-menu-item danger" onclick="QQFriends.blockUser('${contact.user_id}')">
            🚫 加入黑名单
        </div>
    `;

    document.body.appendChild(menu);

    // 点击其他地方关闭
    setTimeout(() => {
        document.addEventListener('click', QQFriends.removeContextMenu, { once: true });
    }, 0);
};

/**
 * 显示分组右键菜单
 */
QQFriends.showGroupContextMenu = function (event, group) {
    QQFriends.removeContextMenu();

    const menu = document.createElement('div');
    menu.className = 'context-menu';
    menu.style.left = event.clientX + 'px';
    menu.style.top = event.clientY + 'px';

    menu.innerHTML = `
        <div class="context-menu-header">${escapeHtml(group.name)}</div>
        <div class="context-menu-item" onclick="QQFriends.renameGroup('${group.id}', prompt('输入新名称：', '${escapeHtml(group.name)}'))">
            ✏️ 重命名
        </div>
        <div class="context-menu-divider"></div>
        <div class="context-menu-item danger" onclick="QQFriends.deleteGroup('${group.id}')">
            🗑️ 删除分组
        </div>
    `;

    document.body.appendChild(menu);

    setTimeout(() => {
        document.addEventListener('click', QQFriends.removeContextMenu, { once: true });
    }, 0);
};

/**
 * 移除右键菜单
 */
QQFriends.removeContextMenu = function () {
    const menu = document.querySelector('.context-menu');
    if (menu) menu.remove();
};

/**
 * 显示设置备注弹窗
 */
QQFriends.showSetNoteDialog = function (userId, currentName) {
    const note = prompt(`为 "${currentName}" 设置备注名：`);
    if (note !== null) {
        QQFriends.setFriendNote(userId, note.trim());
    }
};

/**
 * 显示移动分组弹窗
 */
QQFriends.showMoveGroupDialog = function (userId, displayName) {
    const groupsList = QQFriends.groups.map(g =>
        `<option value="${g.id}">${escapeHtml(g.name)}</option>`
    ).join('');

    const overlay = document.createElement('div');
    overlay.className = 'dialog-overlay';
    overlay.innerHTML = `
        <div class="dialog-box dialog-small">
            <div class="dialog-header">
                <h3>📂 移动好友</h3>
                <button class="dialog-close" onclick="this.closest('.dialog-overlay').remove()">&times;</button>
            </div>
            <div class="dialog-body">
                <p>将 <strong>${escapeHtml(displayName)}</strong> 移动至：</p>
                <select id="move-group-select" class="form-select">
                    ${groupsList}
                </select>
            </div>
            <div class="dialog-footer">
                <button class="btn btn-secondary" onclick="this.closest('.dialog-overlay').remove()">取消</button>
                <button class="btn btn-primary" onclick="QQFriends.moveToGroup('${userId}', $('#move-group-select').value); this.closest('.dialog-overlay').remove()">确定</button>
            </div>
        </div>
    `;
    document.body.appendChild(overlay);
};

// === 初始化 ===
QQFriends.init = function () {
    QQFriends.loadGroups();
    QQFriends.loadRecentContacts();
    QQFriends.loadPendingRequests();
    QQFriends.loadBlacklist();
};

window.QQFriends = QQFriends;
