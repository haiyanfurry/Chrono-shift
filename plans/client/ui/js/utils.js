/**
 * Chrono-shift 工具函数
 */

const Chrono = window.Chrono || {};

// === DOM 工具 ===
function $(selector) {
    return document.querySelector(selector);
}

function $$(selector) {
    return document.querySelectorAll(selector);
}

function createElement(tag, className, innerHTML) {
    const el = document.createElement(tag);
    if (className) el.className = className;
    if (innerHTML) el.innerHTML = innerHTML;
    return el;
}

// === 时间格式化 ===
function formatTime(timestamp) {
    if (!timestamp) return '';
    const date = new Date(timestamp);
    const now = new Date();
    const diff = now - date;
    
    // 今天
    if (date.toDateString() === now.toDateString()) {
        return date.toLocaleTimeString('zh-CN', { hour: '2-digit', minute: '2-digit' });
    }
    // 昨天
    const yesterday = new Date(now);
    yesterday.setDate(yesterday.getDate() - 1);
    if (date.toDateString() === yesterday.toDateString()) {
        return '昨天 ' + date.toLocaleTimeString('zh-CN', { hour: '2-digit', minute: '2-digit' });
    }
    // 今年
    if (date.getFullYear() === now.getFullYear()) {
        return date.toLocaleDateString('zh-CN', { month: 'short', day: 'numeric' });
    }
    // 更早
    return date.toLocaleDateString('zh-CN', { year: 'numeric', month: 'short', day: 'numeric' });
}

// === 防抖 ===
function debounce(fn, delay = 300) {
    let timer = null;
    return function (...args) {
        clearTimeout(timer);
        timer = setTimeout(() => fn.apply(this, args), delay);
    };
}

// === HTML 转义（防 XSS） ===
function escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}

// === 生成唯一 ID ===
function generateId() {
    return Date.now().toString(36) + Math.random().toString(36).substr(2, 9);
}

// === 通知 ===
function showNotification(message, type = 'info') {
    const container = $('#notification-container');
    if (!container) {
        const div = document.createElement('div');
        div.id = 'notification-container';
        div.style.cssText = 'position:fixed;top:16px;right:16px;z-index:9999;display:flex;flex-direction:column;gap:8px;';
        document.body.appendChild(div);
    }
    
    const notif = document.createElement('div');
    notif.className = `notification notification-${type}`;
    notif.textContent = message;
    notif.style.cssText = `
        padding: 12px 20px;
        border-radius: var(--border-radius-md, 10px);
        background: var(--color-bg-primary, #fff);
        color: var(--color-text-primary, #1A1A2E);
        box-shadow: 0 4px 16px rgba(0,0,0,0.12);
        animation: fadeIn 0.25s ease;
        font-size: var(--font-size-sm, 13px);
        border-left: 4px solid var(--color-${type === 'error' ? 'error' : type === 'success' ? 'success' : 'info'});
        max-width: 360px;
    `;
    
    const container_el = $('#notification-container');
    container_el.appendChild(notif);
    
    setTimeout(() => {
        notif.style.opacity = '0';
        notif.style.transition = 'opacity 0.3s ease';
        setTimeout(() => notif.remove(), 300);
    }, 3000);
}

window.Chrono = Chrono;
