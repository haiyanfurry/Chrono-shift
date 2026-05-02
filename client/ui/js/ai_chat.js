/**
 * Chrono-shift AI 聊天模块
 *
 * 在聊天界面中集成 AI 对话功能
 * 支持 OpenAI 兼容 API 和自定义 API
 */
const AIChat = window.AIChat || {};

// === 状态 ===
AIChat.enabled = false;
AIChat.conversations = {};
AIChat.currentConversationId = null;
AIChat.isProcessing = false;

// === AI 配置 ===
AIChat.config = {
    provider: 'openai',       // 'openai' | 'custom'
    apiEndpoint: '',
    apiKey: '',
    model: 'gpt-3.5-turbo',
    maxTokens: 2048,
    temperature: 0.7,
    systemPrompt: '你是一个有用的 AI 助手。'
};

// === 初始化 ===
AIChat.init = function () {
    console.log('[AIChat] 初始化 AI 聊天模块');
    
    // 从 localStorage 加载配置
    AIChat.loadConfig();
    
    // 注入 AI 聊天按钮到聊天头部
    AIChat.injectAIButton();
    
    // 创建 AI 聊天面板
    AIChat.createAIPanel();
};

// === 加载/保存配置 ===
AIChat.loadConfig = function () {
    try {
        const saved = localStorage.getItem('chrono_ai_config');
        if (saved) {
            const parsed = JSON.parse(saved);
            Object.assign(AIChat.config, parsed);
            AIChat.enabled = AIChat.config.apiEndpoint && AIChat.config.apiKey;
        }
    } catch (e) {
        console.warn('[AIChat] 加载配置失败:', e);
    }
};

AIChat.saveConfig = function () {
    try {
        localStorage.setItem('chrono_ai_config', JSON.stringify(AIChat.config));
        AIChat.enabled = AIChat.config.apiEndpoint && AIChat.config.apiKey;
    } catch (e) {
        console.warn('[AIChat] 保存配置失败:', e);
    }
};

// === 注入 AI 按钮 ===
AIChat.injectAIButton = function () {
    const header = $('#chat-header');
    if (!header) return;
    
    // 检查是否已有 AI 按钮
    if (header.querySelector('.btn-ai-chat')) return;
    
    const aiBtn = document.createElement('button');
    aiBtn.className = 'btn btn-sm btn-ai-chat';
    aiBtn.title = 'AI 助手';
    aiBtn.innerHTML = '🤖 AI';
    aiBtn.onclick = function () { AIChat.togglePanel(); };
    
    header.appendChild(aiBtn);
};

// === 创建 AI 面板 ===
AIChat.createAIPanel = function () {
    // 检查是否已存在
    if (document.getElementById('ai-chat-panel')) return;
    
    const panel = document.createElement('div');
    panel.id = 'ai-chat-panel';
    panel.className = 'ai-chat-panel';
    panel.innerHTML = `
        <div class="ai-panel-header">
            <span class="ai-panel-title">🤖 AI 助手</span>
            <div class="ai-panel-actions">
                <button class="btn btn-sm btn-secondary" onclick="AIChat.clearConversation()" title="清空对话">🗑️</button>
                <button class="btn btn-sm btn-secondary" onclick="AIChat.hidePanel()" title="关闭">✕</button>
            </div>
        </div>
        <div class="ai-panel-messages" id="ai-panel-messages">
            <div class="ai-welcome">
                <div class="ai-avatar">🤖</div>
                <p>你好！我是 AI 助手</p>
                <p class="ai-hint">请先在设置中配置 AI API</p>
            </div>
        </div>
        <div class="ai-panel-input">
            <textarea class="ai-input" id="ai-input" placeholder="输入消息..." rows="2"></textarea>
            <button class="btn btn-primary btn-ai-send" id="btn-ai-send" onclick="AIChat.send()">发送</button>
        </div>
    `;
    
    // 插入到聊天消息区域后面
    const chatArea = document.querySelector('.chat-input-area');
    if (chatArea && chatArea.parentNode) {
        chatArea.parentNode.insertBefore(panel, chatArea);
    }
    
    // 绑定键盘事件
    const input = $('#ai-input');
    if (input) {
        input.addEventListener('keydown', function (e) {
            if (e.key === 'Enter' && !e.shiftKey) {
                e.preventDefault();
                AIChat.send();
            }
        });
    }
};

// === 显示/隐藏 AI 面板 ===
AIChat.togglePanel = function () {
    const panel = $('#ai-chat-panel');
    if (!panel) return;
    
    const isVisible = panel.style.display !== 'none';
    panel.style.display = isVisible ? 'none' : 'flex';
    
    if (!isVisible) {
        const input = $('#ai-input');
        if (input) input.focus();
    }
};

AIChat.hidePanel = function () {
    const panel = $('#ai-chat-panel');
    if (panel) panel.style.display = 'none';
};

AIChat.showPanel = function () {
    const panel = $('#ai-chat-panel');
    if (panel) panel.style.display = 'flex';
};

// === 发送消息 ===
AIChat.send = async function () {
    const input = $('#ai-input');
    if (!input) return;
    
    const content = input.value.trim();
    if (!content) return;
    
    if (!AIChat.enabled) {
        showNotification('请先在设置中配置 AI API', 'error');
        return;
    }
    
    if (AIChat.isProcessing) {
        showNotification('AI 正在处理中...', 'info');
        return;
    }
    
    input.value = '';
    AIChat.isProcessing = true;
    
    // 添加用户消息
    AIChat.addMessage('user', content);
    
    // 添加 AI 正在输入的指示
    const typingDiv = document.createElement('div');
    typingDiv.className = 'ai-message ai-message-assistant ai-typing';
    typingDiv.innerHTML = '<div class="ai-avatar">🤖</div><div class="ai-bubble"><span class="typing-dots">思考中...</span></div>';
    const messagesContainer = $('#ai-panel-messages');
    if (messagesContainer) {
        messagesContainer.appendChild(typingDiv);
        messagesContainer.scrollTop = messagesContainer.scrollHeight;
    }
    
    // 发送请求到后台 API 或直接请求外部 API
    try {
        const response = await AIChat.callAPI(content);
        
        // 移除 typing 指示
        if (typingDiv.parentNode) {
            typingDiv.remove();
        }
        
        // 添加 AI 回复
        AIChat.addMessage('assistant', response);
    } catch (error) {
        // 移除 typing 指示
        if (typingDiv.parentNode) {
            typingDiv.remove();
        }
        AIChat.addMessage('assistant', '抱歉，请求失败: ' + error.message);
    } finally {
        AIChat.isProcessing = false;
        const sendBtn = $('#btn-ai-send');
        if (sendBtn) sendBtn.disabled = false;
    }
};

// === 调用 AI API ===
AIChat.callAPI = async function (content) {
    const config = AIChat.config;
    const messages = [
        { role: 'system', content: config.systemPrompt }
    ];
    
    // 添加历史消息（最近 10 条）
    const conv = AIChat.conversations[AIChat.currentConversationId];
    if (conv && conv.messages) {
        const recentMessages = conv.messages.slice(-10);
        recentMessages.forEach(m => {
            messages.push({ role: m.role, content: m.content });
        });
    }
    
    // 添加当前消息
    messages.push({ role: 'user', content: content });
    
    if (config.provider === 'openai' || config.provider === 'custom') {
        const endpoint = config.apiEndpoint.endsWith('/chat/completions')
            ? config.apiEndpoint
            : config.apiEndpoint.replace(/\/+$/, '') + '/v1/chat/completions';
        
        const response = await fetch(endpoint, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
                'Authorization': 'Bearer ' + config.apiKey
            },
            body: JSON.stringify({
                model: config.model,
                messages: messages,
                max_tokens: config.maxTokens,
                temperature: config.temperature
            })
        });
        
        if (!response.ok) {
            throw new Error('HTTP ' + response.status + ': ' + response.statusText);
        }
        
        const data = await response.json();
        if (data.choices && data.choices[0] && data.choices[0].message) {
            return data.choices[0].message.content;
        } else {
            throw new Error('API 返回格式异常');
        }
    }
    
    throw new Error('未支持的 AI 提供商: ' + config.provider);
};

// === 添加消息到面板 ===
AIChat.addMessage = function (role, content) {
    const container = $('#ai-panel-messages');
    if (!container) return;
    
    // 移除欢迎信息
    const welcome = container.querySelector('.ai-welcome');
    if (welcome) welcome.style.display = 'none';
    
    const msgDiv = document.createElement('div');
    msgDiv.className = 'ai-message ai-message-' + role;
    msgDiv.innerHTML = `
        <div class="ai-avatar">${role === 'user' ? '👤' : '🤖'}</div>
        <div class="ai-bubble">${AIChat.escapeHtml(content)}</div>
    `;
    container.appendChild(msgDiv);
    container.scrollTop = container.scrollHeight;
    
    // 保存到会话
    if (!AIChat.currentConversationId) {
        AIChat.currentConversationId = 'conv_' + Date.now();
    }
    if (!AIChat.conversations[AIChat.currentConversationId]) {
        AIChat.conversations[AIChat.currentConversationId] = { messages: [] };
    }
    AIChat.conversations[AIChat.currentConversationId].messages.push({
        role: role,
        content: content,
        timestamp: Date.now()
    });
};

// === 清空对话 ===
AIChat.clearConversation = function () {
    const container = $('#ai-panel-messages');
    if (container) {
        container.innerHTML = `
            <div class="ai-welcome">
                <div class="ai-avatar">🤖</div>
                <p>对话已清空</p>
                <p class="ai-hint">开始新的对话吧</p>
            </div>
        `;
    }
    
    if (AIChat.currentConversationId) {
        delete AIChat.conversations[AIChat.currentConversationId];
        AIChat.currentConversationId = null;
    }
};

// === 智能回复 ===
AIChat.getSmartReply = async function (messageContent) {
    if (!AIChat.enabled) return [];
    
    try {
        const config = AIChat.config;
        const endpoint = config.apiEndpoint.replace(/\/+$/, '') + '/v1/chat/completions';
        
        const response = await fetch(endpoint, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
                'Authorization': 'Bearer ' + config.apiKey
            },
            body: JSON.stringify({
                model: config.model,
                messages: [
                    { role: 'system', content: '你是一个智能回复助手。请根据用户的消息，生成 3 条简短、自然的回复建议。以 JSON 数组格式返回，如 ["回复1","回复2","回复3"]' },
                    { role: 'user', content: messageContent }
                ],
                max_tokens: 150,
                temperature: 0.3
            })
        });
        
        if (!response.ok) return [];
        
        const data = await response.json();
        if (data.choices && data.choices[0] && data.choices[0].message) {
            try {
                const suggestions = JSON.parse(data.choices[0].message.content);
                return Array.isArray(suggestions) ? suggestions.slice(0, 3) : [];
            } catch (e) {
                return [];
            }
        }
    } catch (e) {
        console.warn('[AIChat] 智能回复获取失败:', e);
    }
    
    return [];
};

// === 工具函数 ===
AIChat.escapeHtml = function (text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
};

// === 测试连接 ===
AIChat.testConnection = async function () {
    const config = AIChat.config;
    if (!config.apiEndpoint || !config.apiKey) {
        return { success: false, message: '请先填写 API 端点和密钥' };
    }
    
    try {
        const endpoint = config.apiEndpoint.replace(/\/+$/, '') + '/v1/models';
        const response = await fetch(endpoint, {
            headers: {
                'Authorization': 'Bearer ' + config.apiKey
            }
        });
        
        if (response.ok) {
            const data = await response.json();
            return { success: true, message: '连接成功！可用模型: ' + (data.data ? data.data.length : '未知') + ' 个' };
        } else {
            return { success: false, message: '连接失败: HTTP ' + response.status };
        }
    } catch (e) {
        return { success: false, message: '连接失败: ' + e.message };
    }
};

window.AIChat = AIChat;
