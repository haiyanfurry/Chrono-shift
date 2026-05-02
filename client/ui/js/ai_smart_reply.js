/**
 * Chrono-shift AI 智能回复模块
 *
 * 在聊天消息上提供智能回复建议
 */
const AISmartReply = window.AISmartReply || {};

// === 状态 ===
AISmartReply.enabled = false;
AISmartReply.currentSuggestions = [];

// === 初始化 ===
AISmartReply.init = function () {
    console.log('[AISmartReply] 初始化智能回复模块');
    
    // 延迟加载，确保 AI 配置已加载
    if (window.AIChat) {
        AISmartReply.enabled = AIChat.enabled;
    }
};

// === 为消息生成智能回复建议 ===
AISmartReply.generate = async function (messageContent, messageElement) {
    if (!AISmartReply.enabled || !window.AIChat) return;
    
    try {
        const suggestions = await AIChat.getSmartReply(messageContent);
        if (suggestions.length > 0) {
            AISmartReply.showSuggestions(suggestions, messageElement);
        }
    } catch (e) {
        console.warn('[AISmartReply] 生成失败:', e);
    }
};

// === 显示回复建议 ===
AISmartReply.showSuggestions = function (suggestions, messageElement) {
    // 移除已有的建议
    AISmartReply.removeSuggestions();
    
    AISmartReply.currentSuggestions = suggestions;
    
    const container = document.createElement('div');
    container.className = 'smart-reply-container';
    container.id = 'smart-reply-container';
    
    const label = document.createElement('div');
    label.className = 'smart-reply-label';
    label.textContent = '💡 快速回复';
    container.appendChild(label);
    
    suggestions.forEach((text, index) => {
        const btn = document.createElement('button');
        btn.className = 'btn btn-sm smart-reply-btn';
        btn.textContent = text;
        btn.onclick = function () {
            AISmartReply.useSuggestion(text);
        };
        container.appendChild(btn);
    });
    
    // 插入到聊天输入框上方
    const inputArea = document.querySelector('.chat-input-area');
    if (inputArea) {
        inputArea.parentNode.insertBefore(container, inputArea);
    }
};

// === 使用建议回复 ===
AISmartReply.useSuggestion = function (text) {
    const input = $('#chat-input');
    if (input) {
        input.value = text;
        Chat.send();
    }
    AISmartReply.removeSuggestions();
};

// === 移除建议 ===
AISmartReply.removeSuggestions = function () {
    const container = $('#smart-reply-container');
    if (container) container.remove();
    AISmartReply.currentSuggestions = [];
};

window.AISmartReply = AISmartReply;
