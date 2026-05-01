/**
 * Chrono-shift 主题引擎
 * 
 * 核心功能：动态加载和切换社区模板主题
 * 通过覆盖 CSS 变量的方式实现即时换肤
 */

const ThemeEngine = window.ThemeEngine || {};

// === 状态 ===
ThemeEngine.currentTheme = 'default';
ThemeEngine.customStyles = {};

// === 加载主题 ===
ThemeEngine.loadTheme = async function (themeId, cssContent) {
    try {
        if (themeId === 'default') {
            ThemeEngine.resetToDefault();
            return;
        }
        
        // 方式1: 直接应用 CSS 变量覆盖
        if (cssContent) {
            ThemeEngine.applyCustomCSS(cssContent);
            ThemeEngine.currentTheme = themeId;
            return;
        }
        
        // 方式2: 从服务器加载主题文件
        const response = await fetch(`${API.BASE_URL}/api/templates/download?id=${themeId}`);
        if (response.ok) {
            const cssText = await response.text();
            ThemeEngine.applyCustomCSS(cssText);
            ThemeEngine.currentTheme = themeId;
        }
    } catch (error) {
        console.error('[ThemeEngine] 加载主题失败:', error);
        showNotification('主题加载失败', 'error');
    }
};

// === 应用自定义 CSS ===
ThemeEngine.applyCustomCSS = function (cssText) {
    // 移除旧的样式
    const oldStyle = document.getElementById('theme-custom-style');
    if (oldStyle) {
        oldStyle.remove();
    }
    
    // 创建新的样式标签
    const style = document.createElement('style');
    style.id = 'theme-custom-style';
    style.textContent = cssText;
    document.head.appendChild(style);
};

// === 重置为默认主题 ===
ThemeEngine.resetToDefault = function () {
    const customStyle = document.getElementById('theme-custom-style');
    if (customStyle) {
        customStyle.remove();
    }
    
    // 确保默认主题样式被应用
    const themeLink = document.getElementById('theme-stylesheet');
    if (themeLink) {
        themeLink.href = 'css/themes/default.css';
        // 强制重新加载
        themeLink.href = 'css/themes/default.css?' + Date.now();
    }
    
    ThemeEngine.currentTheme = 'default';
    ThemeEngine.customStyles = {};
    
    showNotification('已恢复默认纯白主题', 'success');
};

// === 预览主题 ===
ThemeEngine.previewTheme = function (cssContent) {
    // 预览模式，不保存状态
    const oldStyle = document.getElementById('theme-preview-style');
    if (oldStyle) {
        oldStyle.remove();
    }
    
    const style = document.createElement('style');
    style.id = 'theme-preview-style';
    style.textContent = cssContent;
    document.head.appendChild(style);
};

// === 取消预览 ===
ThemeEngine.cancelPreview = function () {
    const previewStyle = document.getElementById('theme-preview-style');
    if (previewStyle) {
        previewStyle.remove();
    }
};

// === 导出当前主题 ===
ThemeEngine.exportCurrentTheme = function () {
    const computedStyle = getComputedStyle(document.documentElement);
    const variables = {};
    
    // 收集所有 CSS 变量
    for (let i = 0; i < computedStyle.length; i++) {
        const prop = computedStyle[i];
        if (prop.startsWith('--')) {
            variables[prop] = computedStyle.getPropertyValue(prop).trim();
        }
    }
    
    return variables;
};

window.ThemeEngine = ThemeEngine;
