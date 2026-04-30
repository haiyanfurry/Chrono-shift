/**
 * Chrono-shift 联系人管理
 */

const Contacts = window.Contacts || {};

// === 状态 ===
Contacts.list = [];

// === 加载联系人列表 ===
Contacts.load = async function () {
    const result = await API.getFriends();
    
    if (result.status === 'ok' && result.data) {
        Contacts.list = result.data.friends || [];
        Contacts.renderSidebar();
        Contacts.renderGrid();
    }
};

// === 渲染侧边栏联系人列表 ===
Contacts.renderSidebar = function () {
    const container = $('#contact-list');
    container.innerHTML = '';
    
    if (Contacts.list.length === 0) {
        container.innerHTML = '<div class="loading">暂无联系人</div>';
        return;
    }
    
    Contacts.list.forEach(contact => {
        const item = document.createElement('div');
        item.className = 'contact-item';
        item.dataset.id = contact.user_id;
        item.onclick = () => Chat.open(contact.user_id, contact.username);
        
        item.innerHTML = `
            <div class="user-avatar">
                <img src="${contact.avatar_url || 'assets/images/default_avatar.png'}" alt="头像">
            </div>
            <div class="contact-info">
                <div class="contact-name">${escapeHtml(contact.nickname || contact.username)}</div>
                <div class="contact-preview">${contact.last_message || ''}</div>
            </div>
            <div class="contact-time">${formatTime(contact.last_time)}</div>
        `;
        
        container.appendChild(item);
    });
};

// === 渲染联系人网格 ===
Contacts.renderGrid = function () {
    const container = $('#contacts-grid');
    if (!container) return;
    
    container.innerHTML = '';
    
    Contacts.list.forEach(contact => {
        const card = document.createElement('div');
        card.className = 'contact-card';
        card.onclick = () => {
            switchTab('chat');
            Chat.open(contact.user_id, contact.username);
        };
        
        card.innerHTML = `
            <div class="user-avatar" style="width:56px;height:56px;">
                <img src="${contact.avatar_url || 'assets/images/default_avatar.png'}" alt="头像">
            </div>
            <div class="contact-name">${escapeHtml(contact.nickname || contact.username)}</div>
            <div class="contact-status">${contact.online ? '🟢 在线' : '⚪ 离线'}</div>
        `;
        
        container.appendChild(card);
    });
};

window.Contacts = Contacts;
