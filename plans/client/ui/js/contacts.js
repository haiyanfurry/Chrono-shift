/**
 * Chrono-shift QQ 风格联系人管理
 * 集成好友分组、最近联系人、搜索
 */

const Contacts = window.Contacts || {};

// === 状态 ===
Contacts.list = [];
Contacts.groupsVisible = true;

// === 加载联系人列表 ===
Contacts.load = async function () {
    const result = await API.getFriends();
    
    if (result.status === 'ok' && result.data) {
        Contacts.list = result.data.friends || [];
        Contacts.renderSidebar();
        Contacts.renderGrid();
        
        // 更新 QQ 风格分组
        if (window.QQFriends) {
            QQFriends.loadGroups();
            QQFriends.loadRecentContacts();
        }
        return true;
    }
    return false;
};

// === 渲染侧边栏联系人列表（QQ 风格） ===
Contacts.renderSidebar = function () {
    const container = $('#contact-list');
    container.innerHTML = '';
    
    // 渲染分组列表
    if (window.QQFriends && QQFriends.groups.length > 0) {
        QQFriends.renderGroups();
    }
    
    // 如果当前有选中的分组，渲染分组成员
    if (window.QQFriends && QQFriends.currentGroup) {
        if (QQFriends.currentGroup === 'recent') {
            QQFriends.showRecentContacts();
        } else {
            QQFriends.filterByGroup(QQFriends.currentGroup);
        }
        return;
    }
    
    // 默认显示所有联系人
    Contacts.renderAllContacts(container);
};

// === 渲染所有联系人（默认视图） ===
Contacts.renderAllContacts = function (container) {
    if (!container) container = $('#contact-list');
    container.innerHTML = '';
    
    if (Contacts.list.length === 0) {
        container.innerHTML = '<div class="loading">暂无联系人</div>';
        return;
    }
    
    Contacts.list.forEach(contact => {
        const item = Contacts.createContactItem(contact);
        container.appendChild(item);
    });
};

// === QQ 风格联系人项 ===
Contacts.createContactItem = function (contact) {
    const item = document.createElement('div');
    item.className = 'contact-item qq-contact-item';
    item.dataset.id = contact.user_id;
    
    const displayName = contact.note || contact.nickname || contact.username || '未知用户';
    const avatarSrc = escapeHtml(contact.avatar_url || 'assets/images/default_avatar.png');
    const lastMsg = escapeHtml(contact.last_message || contact.signature || '');
    const timeText = formatTime(contact.last_time);
    const onlineClass = contact.online ? 'online' : 'offline';
    
    item.innerHTML = `
        <div class="contact-avatar-wrap">
            <img src="${avatarSrc}" alt="头像"
                 onerror="this.src='assets/images/default_avatar.png'">
            <span class="contact-status-dot ${onlineClass}"></span>
        </div>
        <div class="contact-info">
            <div class="contact-name-row">
                <span class="contact-name">${escapeHtml(displayName)}</span>
                ${contact.note ? '<span class="contact-note-badge">备注</span>' : ''}
            </div>
            <div class="contact-preview">${lastMsg}</div>
        </div>
        <div class="contact-time">${timeText}</div>
    `;
    
    // 左键点击打开聊天
    item.onclick = () => {
        if (window.Chat && typeof Chat.open === 'function') {
            Chat.open(contact.user_id, displayName);
        }
    };
    
    // 右键菜单
    if (window.QQFriends) {
        item.oncontextmenu = (e) => {
            e.preventDefault();
            QQFriends.showContactContextMenu(e, contact);
        };
    }
    
    return item;
};

// === 渲染联系人网格 ===
Contacts.renderGrid = function () {
    const container = $('#contacts-grid');
    if (!container) return;
    
    container.innerHTML = '';
    
    if (Contacts.list.length === 0) {
        container.innerHTML = '<div class="loading">暂无联系人，点击右上角添加好友</div>';
        return;
    }
    
    Contacts.list.forEach(contact => {
        const card = document.createElement('div');
        card.className = 'contact-card';
        card.onclick = () => {
            switchTab('chat');
            Chat.open(contact.user_id, contact.note || contact.nickname || contact.username);
        };
        
        const displayName = contact.note || contact.nickname || contact.username || '未知用户';
        
        card.innerHTML = `
            <div class="user-avatar" style="width:56px;height:56px;">
                <img src="${escapeHtml(contact.avatar_url || 'assets/images/default_avatar.png')}" alt="头像"
                     onerror="this.src='assets/images/default_avatar.png'">
                <span class="contact-status-dot ${contact.online ? 'online' : 'offline'}" 
                      style="position:absolute;bottom:2px;right:2px;"></span>
            </div>
            <div class="contact-name">${escapeHtml(displayName)}</div>
            <div class="contact-status">${contact.online ? '🟢 在线' : '⚪ 离线'}</div>
        `;
        
        container.appendChild(card);
    });
};

// === 搜索联系人 ===
Contacts.search = async function (keyword) {
    if (!keyword.trim()) {
        Contacts.renderSidebar();
        return;
    }
    
    const result = await API.searchUsers(keyword);
    const container = $('#contact-list');
    container.innerHTML = '';
    
    if (result.status === 'ok' && result.data) {
        (result.data.users || []).forEach(user => {
            const item = document.createElement('div');
            item.className = 'contact-item qq-contact-item';
            item.innerHTML = `
                <div class="contact-avatar-wrap">
                    <img src="${escapeHtml(user.avatar_url || 'assets/images/default_avatar.png')}" alt="头像"
                         onerror="this.src='assets/images/default_avatar.png'">
                    <span class="contact-status-dot offline"></span>
                </div>
                <div class="contact-info">
                    <div class="contact-name-row">
                        <span class="contact-name">${escapeHtml(user.nickname || user.username)}</span>
                    </div>
                    <div class="contact-preview">点击添加好友</div>
                </div>
            `;
            
            item.onclick = () => {
                if (window.QQFriends) {
                    QQFriends.applyFriend(user.user_id);
                } else {
                    showNotification('添加好友功能开发中', 'info');
                }
            };
            
            container.appendChild(item);
        });
        
        if (result.data.users && result.data.users.length === 0) {
            container.innerHTML = '<div class="loading">未找到用户</div>';
        }
    }
};

window.Contacts = Contacts;
