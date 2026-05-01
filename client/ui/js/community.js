/**
 * Chrono-shift 社区/模板管理
 */

const Community = window.Community || {};

// === 状态 ===
Community.templates = [];

// === 加载模板列表 ===
Community.load = async function () {
    const result = await API.getTemplates();
    
    if (result.status === 'ok' && result.data) {
        Community.templates = result.data.templates || [];
        Community.renderGrid();
    }
};

// === 渲染模板网格 ===
Community.renderGrid = function () {
    const container = $('#template-grid');
    if (!container) return;
    
    container.innerHTML = '';
    
    if (Community.templates.length === 0) {
        container.innerHTML = '<div class="loading">暂无社区模板</div>';
        return;
    }
    
    Community.templates.forEach(template => {
        const card = document.createElement('div');
        card.className = 'template-card';
        
        card.innerHTML = `
            <div class="template-preview">
                ${template.preview_url 
                    ? `<img src="${template.preview_url}" alt="${escapeHtml(template.name)}">`
                    : '🎨'}
            </div>
            <div class="template-info">
                <div class="template-name">${escapeHtml(template.name)}</div>
                <div class="template-author">作者: ${escapeHtml(template.author_name || '未知')}</div>
                <div class="template-stats">
                    <span>📥 ${template.downloads || 0} 次下载</span>
                </div>
            </div>
            <div class="template-actions">
                <button class="btn btn-secondary btn-small" onclick="Community.preview(${template.id})">
                    预览
                </button>
                <button class="btn btn-primary btn-small" onclick="Community.apply(${template.id})">
                    应用
                </button>
            </div>
        `;
        
        container.appendChild(card);
    });
};

// === 预览模板 ===
Community.preview = async function (templateId) {
    try {
        const response = await fetch(`${API.BASE_URL}/api/templates/download?id=${templateId}`);
        if (response.ok) {
            const cssText = await response.text();
            ThemeEngine.previewTheme(cssText);
            showNotification('预览模式 - 点击"应用"以保存', 'info');
        }
    } catch (error) {
        showNotification('预览加载失败', 'error');
    }
};

// === 应用模板 ===
Community.apply = async function (templateId) {
    ThemeEngine.cancelPreview();
    
    const result = await API.applyTemplate(templateId);
    
    if (result.status === 'ok') {
        // 加载并应用主题
        await ThemeEngine.loadTheme(templateId);
        showNotification('主题已应用', 'success');
    } else {
        showNotification(result.message || '应用失败', 'error');
    }
};

// === 上传模板 ===
Community.showUploadDialog = function () {
    const name = prompt('请输入模板名称:');
    if (!name) return;
    
    // 创建文件选择器
    const input = document.createElement('input');
    input.type = 'file';
    input.accept = '.css';
    
    input.onchange = async function (e) {
        const file = e.target.files[0];
        if (!file) return;
        
        const reader = new FileReader();
        reader.onload = async function () {
            const content = reader.result;
            // Phase 5 实现上传逻辑
            showNotification('上传功能开发中 - Phase 5', 'info');
        };
        reader.readAsText(file);
    };
    
    input.click();
};

window.Community = Community;
