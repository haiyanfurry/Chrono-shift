/**
 * Chrono-shift QQ 风格聊天模块
 * 支持私聊、群聊、文件消息、表情
 */

const Chat = window.Chat || {};

// === 状态 ===
Chat.currentPartner = null;
Chat.messages = [];
Chat.wsConnection = null;

// === 打开聊天 ===
Chat.open = function (userId, username) {
    Chat.currentPartner = { id: userId, name: username, type: 'user' };
    
    // 更新聊天头部
    const header = $('#chat-header');
    if (header) {
        header.innerHTML = `
            <span class="chat-partner">${escapeHtml(username)}</span>
            <span class="chat-partner-status">在线</span>
        `;
    }
    
    $('#chat-input').disabled = false;
    $('#btn-send').disabled = false;
    $('#chat-input').focus();
    
    // 加载消息历史
    Chat.loadMessages(userId);
};

// === 加载消息历史 ===
Chat.loadMessages = async function (userId) {
    const result = await API.getMessages(userId);
    const messagesContainer = $('#chat-messages');
    messagesContainer.innerHTML = '';
    
    if (result.status === 'ok' && result.data && result.data.messages) {
        Chat.messages = result.data.messages;
        
        // 添加日期分隔线
        let lastDate = null;
        Chat.messages.forEach(msg => {
            const msgDate = new Date(msg.timestamp).toDateString();
            if (msgDate !== lastDate) {
                Chat.renderDateDivider(msg.timestamp);
                lastDate = msgDate;
            }
            Chat.renderMessage(msg);
        });
    }
    
    messagesContainer.scrollTop = messagesContainer.scrollHeight;
};

// === 渲染日期分隔线 ===
Chat.renderDateDivider = function (timestamp) {
    const container = $('#chat-messages');
    const divider = document.createElement('div');
    divider.className = 'date-divider';
    divider.textContent = formatTime(timestamp);
    container.appendChild(divider);
};

// === 渲染消息（支持文本、文件、图片） ===
Chat.renderMessage = function (msg) {
    const container = $('#chat-messages');
    const isSelf = msg.from_id === Auth.currentUser?.user_id;
    
    const messageDiv = document.createElement('div');
    messageDiv.className = `message ${isSelf ? 'message-self' : 'message-other'} fade-in`;
    
    let contentHtml = '';
    
    if (msg.file) {
        // 文件消息
        const fileType = msg.file.file_type || 'default';
        const isImage = fileType === 'image';
        const fileUrl = msg.file.file_url || msg.file.url || '';
        const fileName = msg.file.file_name || '文件';
        const fileSize = msg.file.file_size ? QQFile.formatSize(msg.file.file_size) : '';
        const fileIcon = QQFile.TYPE_ICONS[fileType] || '📎';
        
        if (isImage && fileUrl) {
            // 图片消息 - 缩略图显示
            contentHtml = `
                <div class="message-bubble image-message" onclick="QQFile.previewImage('${escapeHtml(fileUrl)}')">
                    <img src="${escapeHtml(fileUrl)}" alt="图片" style="max-width:200px;max-height:200px;border-radius:8px;cursor:pointer;">
                </div>
            `;
        } else {
            // 文件消息
            contentHtml = `
                <div class="message-bubble">
                    <div class="file-message">
                        <span class="file-icon">${fileIcon}</span>
                        <div class="file-info">
                            <div class="file-name">${escapeHtml(fileName)}</div>
                            <div class="file-size">${fileSize}</div>
                        </div>
                        <a href="${escapeHtml(fileUrl)}" download="${escapeHtml(fileName)}" 
                           class="btn btn-sm btn-primary" onclick="event.stopPropagation()">下载</a>
                    </div>
                </div>
            `;
        }
    } else {
        // 文本消息
        contentHtml = `
            <div class="message-bubble">${escapeHtml(msg.content || '')}</div>
        `;
    }
    
    // 非自己的消息显示头像
    if (!isSelf) {
        messageDiv.innerHTML = `
            <img class="msg-avatar" src="${escapeHtml(msg.avatar_url || 'assets/images/default_avatar.png')}" 
                 onerror="this.src='assets/images/default_avatar.png'">
            <div>
                ${contentHtml}
                <div class="message-time">${formatTime(msg.timestamp)}</div>
            </div>
        `;
    } else {
        messageDiv.innerHTML = `
            <div>
                ${contentHtml}
                <div class="message-time">${formatTime(msg.timestamp)}</div>
            </div>
        `;
    }
    
    container.appendChild(messageDiv);
    container.scrollTop = container.scrollHeight;
};

// === 发送消息（自动判断私聊/群聊） ===
Chat.send = async function () {
    const input = $('#chat-input');
    const content = input.value.trim();
    
    if (!content) return;
    
    // 判断当前是群聊还是私聊
    if (window.QQGroup && QQGroup.currentGroup) {
        // 群聊发送
        QQGroup.sendMessage();
        return;
    }
    
    if (!Chat.currentPartner) {
        showNotification('请先选择一个联系人', 'error');
        return;
    }
    
    // 清空输入
    input.value = '';
    
    // 通过 API 发送
    const result = await API.sendMessage(Chat.currentPartner.id, content);
    
    if (result.status === 'ok') {
        // 渲染已发送消息
        Chat.renderMessage({
            from_id: Auth.currentUser?.user_id,
            to_id: Chat.currentPartner.id,
            content: content,
            timestamp: Date.now(),
            avatar_url: Auth.currentUser?.avatar_url
        });
    } else {
        showNotification('消息发送失败', 'error');
    }
};

// === 处理回车发送 ===
Chat.onKeydown = function (event) {
    if (event.key === 'Enter' && !event.shiftKey) {
        event.preventDefault();
        Chat.send();
    }
};

window.Chat = Chat;
