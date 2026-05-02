/**
 * Chrono-shift QQ 风格表情系统
 * 基础表情、大表情、自定义表情
 */

const QQEmoji = window.QQEmoji || {};

// === QQ 经典表情（文字表情映射） ===
QQEmoji.CLASSIC_EMOJIS = [
    { id: 'smile', char: '😊', name: '微笑' },
    { id: 'laugh', char: '😂', name: '大笑' },
    { id: 'cry', char: '😭', name: '流泪' },
    { id: 'angry', char: '😠', name: '生气' },
    { id: 'love', char: '❤️', name: '爱心' },
    { id: 'cool', char: '😎', name: '酷' },
    { id: 'shy', char: '😳', name: '害羞' },
    { id: 'sleep', char: '😴', name: '睡觉' },
    { id: 'sick', char: '🤒', name: '生病' },
    { id: 'surprise', char: '😱', name: '惊讶' },
    { id: 'kiss', char: '😘', name: '飞吻' },
    { id: 'ok', char: '👌', name: 'OK' },
    { id: 'bye', char: '👋', name: '再见' },
    { id: 'clap', char: '👏', name: '鼓掌' },
    { id: 'fire', char: '🔥', name: '火热' },
    { id: 'star', char: '⭐', name: '星星' },
    { id: 'heart_eyes', char: '😍', name: '色' },
    { id: 'wink', char: '😉', name: '眨眼' },
    { id: 'tongue', char: '😜', name: '吐舌' },
    { id: 'money', char: '🤑', name: '发财' },
    { id: 'sweat', char: '😓', name: '冷汗' },
    { id: 'mask', char: '😷', name: '口罩' },
    { id: 'puke', char: '🤮', name: '吐' },
    { id: 'party', char: '🎉', name: '庆祝' }
];

// === 大表情（动画/贴图） ===
QQEmoji.BIG_EMOJIS = [
    { id: 'big_hug', name: '抱抱', url: '🤗' },
    { id: 'big_birthday', name: '生日', url: '🎂' },
    { id: 'big_fireworks', name: '烟花', url: '🎆' },
    { id: 'big_gift', name: '礼物', url: '🎁' },
    { id: 'big_trophy', name: '奖杯', url: '🏆' },
    { id: 'big_rocket', name: '火箭', url: '🚀' }
];

// === 自定义表情存储 ===
QQEmoji.customEmojis = JSON.parse(localStorage.getItem('qq_emoji_custom') || '[]');

// === 当前激活的标签 ===
QQEmoji.currentTab = 'classic';

// === 创建表情面板 ===
QQEmoji.createPanel = function () {
    // 移除旧面板
    const oldPanel = $('#emoji-panel');
    if (oldPanel) oldPanel.remove();
    
    const panel = document.createElement('div');
    panel.id = 'emoji-panel';
    panel.className = 'emoji-panel';
    
    // 标签栏
    panel.innerHTML = `
        <div class="emoji-tabs">
            <div class="emoji-tab active" data-tab="classic" onclick="QQEmoji.switchTab('classic')">😊 基础</div>
            <div class="emoji-tab" data-tab="big" onclick="QQEmoji.switchTab('big')">🎉 大表情</div>
            <div class="emoji-tab" data-tab="custom" onclick="QQEmoji.switchTab('custom')">📦 收藏</div>
        </div>
        <div id="emoji-grid" class="emoji-grid">
            <!-- 动态渲染 -->
        </div>
    `;
    
    // 插入到输入区域上方
    const chatInput = $('.chat-input-area');
    if (chatInput) {
        chatInput.appendChild(panel);
    }
    
    QQEmoji.renderGrid('classic');
    return panel;
};

// === 切换标签 ===
QQEmoji.switchTab = function (tab) {
    QQEmoji.currentTab = tab;
    
    // 更新标签样式
    document.querySelectorAll('.emoji-tab').forEach(t => {
        t.classList.toggle('active', t.dataset.tab === tab);
    });
    
    QQEmoji.renderGrid(tab);
};

// === 渲染表情网格 ===
QQEmoji.renderGrid = function (tab) {
    const grid = $('#emoji-grid');
    if (!grid) return;
    
    grid.innerHTML = '';
    
    let emojis = [];
    switch (tab) {
        case 'classic':
            emojis = QQEmoji.CLASSIC_EMOJIS;
            break;
        case 'big':
            emojis = QQEmoji.BIG_EMOJIS;
            break;
        case 'custom':
            emojis = QQEmoji.customEmojis;
            break;
    }
    
    if (emojis.length === 0) {
        grid.innerHTML = '<div style="grid-column:1/-1;text-align:center;color:var(--color-text-tertiary);padding:20px;font-size:var(--font-size-sm);">暂无表情</div>';
        if (tab === 'custom') {
            grid.innerHTML += '<button class="btn btn-sm btn-secondary" onclick="QQEmoji.showAddCustomEmoji()" style="grid-column:1/-1;">+ 添加收藏</button>';
        }
        return;
    }
    
    emojis.forEach(emoji => {
        const item = document.createElement('div');
        item.className = 'emoji-item';
        item.title = emoji.name || '';
        item.textContent = emoji.char || emoji.url || '😊';
        
        item.onclick = () => {
            QQEmoji.insertEmoji(emoji.char || emoji.url || '😊');
            QQEmoji.hidePanel();
        };
        
        grid.appendChild(item);
    });
    
    // 自定义标签额外显示添加按钮
    if (tab === 'custom') {
        const addBtn = document.createElement('div');
        addBtn.className = 'emoji-item';
        addBtn.textContent = '➕';
        addBtn.title = '添加自定义表情';
        addBtn.onclick = () => QQEmoji.showAddCustomEmoji();
        grid.appendChild(addBtn);
    }
};

// === 插入表情到输入框 ===
QQEmoji.insertEmoji = function (emojiChar) {
    const input = $('#chat-input');
    if (!input) return;
    
    const start = input.selectionStart;
    const end = input.selectionEnd;
    const text = input.value;
    
    input.value = text.substring(0, start) + emojiChar + text.substring(end);
    input.focus();
    input.selectionStart = input.selectionEnd = start + emojiChar.length;
};

// === 切换表情面板 ===
QQEmoji.togglePanel = function () {
    const panel = $('#emoji-panel') || QQEmoji.createPanel();
    panel.classList.toggle('active');
    
    if (panel.classList.contains('active')) {
        QQEmoji.renderGrid(QQEmoji.currentTab);
    }
};

// === 隐藏表情面板 ===
QQEmoji.hidePanel = function () {
    const panel = $('#emoji-panel');
    if (panel) panel.classList.remove('active');
};

// === 添加自定义表情 ===
QQEmoji.showAddCustomEmoji = function () {
    const emoji = prompt('请输入表情符号（如 😊）：');
    if (emoji) {
        const name = prompt('给这个表情起个名字：') || '自定义';
        QQEmoji.customEmojis.push({
            id: 'custom_' + Date.now(),
            name: name,
            char: emoji
        });
        localStorage.setItem('qq_emoji_custom', JSON.stringify(QQEmoji.customEmojis));
        QQEmoji.renderGrid('custom');
        showNotification('表情已收藏', 'success');
    }
};

// === 在输入框添加表情按钮 ===
QQEmoji.injectEmojiButton = function () {
    const chatInput = $('.chat-input-area');
    if (!chatInput) return;
    
    const emojiBtn = document.createElement('button');
    emojiBtn.type = 'button';
    emojiBtn.className = 'btn btn-icon btn-emoji';
    emojiBtn.innerHTML = '😊';
    emojiBtn.title = '表情';
    emojiBtn.onclick = (e) => {
        e.stopPropagation();
        QQEmoji.togglePanel();
    };
    
    const fileBtn = $('.btn-file-upload');
    if (fileBtn) {
        fileBtn.parentNode.insertBefore(emojiBtn, fileBtn.nextSibling);
    } else {
        const sendBtn = $('#btn-send');
        if (sendBtn) {
            sendBtn.parentNode.insertBefore(emojiBtn, sendBtn);
        }
    }
    
    // 点击面板外关闭
    document.addEventListener('click', (e) => {
        const panel = $('#emoji-panel');
        if (panel && !panel.contains(e.target) && !e.target.closest('.btn-emoji')) {
            panel.classList.remove('active');
        }
    });
};

// === 初始化 ===
QQEmoji.init = function () {
    QQEmoji.injectEmojiButton();
};

window.QQEmoji = QQEmoji;
