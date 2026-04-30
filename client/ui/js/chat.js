/**
 * Chrono-shift 聊天模块
 */

const Chat = window.Chat || {};

// === 状态 ===
Chat.currentPartner = null;
Chat.messages = [];
Chat.wsConnection = null;

// === 打开聊天 ===
Chat.open = function (userId, username) {
    Chat.currentPartner = { id: userId, name: username };
    
    // 更新聊天头部
    $('#chat-header .chat-partner').textContent = username;
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
        Chat.messages.forEach(msg => Chat.renderMessage(msg));
    }
    
    messagesContainer.scrollTop = messagesContainer.scrollHeight;
};

// === 渲染消息 ===
Chat.renderMessage = function (msg) {
    const container = $('#chat-messages');
    const isSelf = msg.from_id === Auth.currentUser?.user_id;
    
    const messageDiv = document.createElement('div');
    messageDiv.className = `message ${isSelf ? 'message-self' : 'message-other'} fade-in`;
    
    messageDiv.innerHTML = `
        <div class="message-bubble">${escapeHtml(msg.content)}</div>
        <div class="message-time">${formatTime(msg.timestamp)}</div>
    `;
    
    container.appendChild(messageDiv);
    container.scrollTop = container.scrollHeight;
};

// === 发送消息 ===
Chat.send = async function () {
    const input = $('#chat-input');
    const content = input.value.trim();
    
    if (!content || !Chat.currentPartner) return;
    
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
            timestamp: Date.now()
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
