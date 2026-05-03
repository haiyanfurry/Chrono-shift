/**
 * Chrono-shift QQ 风格文件分享管理
 * 文件发送、拖拽上传、图片预览、进度条、文件管理
 */

const QQFile = window.QQFile || {};

// === 状态 ===
QQFile.files = [];               // 已收文件列表
QQFile.uploadQueue = [];         // 上传队列
QQFile.currentPreview = null;    // 当前预览图片

// === 文件类型图标映射 ===
QQFile.TYPE_ICONS = {
    'image': '🖼️',
    'video': '🎬',
    'audio': '🎵',
    'pdf': '📄',
    'zip': '📦',
    'doc': '📝',
    'default': '📎'
};

// === 获取文件类型 ===
QQFile.getFileType = function (fileName) {
    const ext = fileName.split('.').pop().toLowerCase();
    if (['jpg', 'jpeg', 'png', 'gif', 'bmp', 'webp', 'svg'].includes(ext)) return 'image';
    if (['mp4', 'avi', 'mov', 'mkv', 'flv'].includes(ext)) return 'video';
    if (['mp3', 'wav', 'ogg', 'flac'].includes(ext)) return 'audio';
    if (['pdf'].includes(ext)) return 'pdf';
    if (['zip', 'rar', '7z', 'tar', 'gz'].includes(ext)) return 'zip';
    if (['doc', 'docx', 'xls', 'xlsx', 'ppt', 'pptx'].includes(ext)) return 'doc';
    return 'default';
};

// === 格式化文件大小 ===
QQFile.formatSize = function (bytes) {
    if (!bytes) return '未知大小';
    const units = ['B', 'KB', 'MB', 'GB'];
    let size = bytes;
    let unitIdx = 0;
    while (size >= 1024 && unitIdx < units.length - 1) {
        size /= 1024;
        unitIdx++;
    }
    return size.toFixed(1) + ' ' + units[unitIdx];
};

// === 发送文件消息 ===
QQFile.sendFile = function (file) {
    if (!Chat.currentPartner && !QQGroup.currentGroup) {
        showNotification('请先选择一个聊天对象', 'error');
        return;
    }
    
    const formData = new FormData();
    formData.append('file', file);
    formData.append('to_user_id', Chat.currentPartner?.id || '');
    formData.append('group_id', QQGroup.currentGroup?.id || '');
    
    // 创建上传进度项
    const uploadId = generateId();
    const uploadItem = {
        id: uploadId,
        name: file.name,
        size: file.size,
        type: QQFile.getFileType(file.name),
        progress: 0,
        status: 'uploading'
    };
    QQFile.uploadQueue.push(uploadItem);
    QQFile.showUploadProgress();
    
    // 上传文件
    API.uploadFile(formData).then(result => {
        if (result.status === 'ok') {
            uploadItem.status = 'done';
            uploadItem.progress = 100;
            
            // 渲染文件消息
            const fileMsg = {
                file_id: result.data?.file_id || uploadId,
                file_name: file.name,
                file_size: file.size,
                file_type: QQFile.getFileType(file.name),
                file_url: result.data?.url || ''
            };
            
            if (Chat.currentPartner) {
                Chat.renderMessage({
                    from_id: Auth.currentUser?.user_id,
                    to_id: Chat.currentPartner.id,
                    content: '',
                    file: fileMsg,
                    timestamp: Date.now()
                });
            } else if (QQGroup.currentGroup) {
                QQGroup.renderGroupMessage({
                    from_id: Auth.currentUser?.user_id,
                    from_nickname: Auth.currentUser?.nickname,
                    content: '',
                    file: fileMsg,
                    timestamp: Date.now(),
                    avatar_url: Auth.currentUser?.avatar_url
                });
            }
        } else {
            uploadItem.status = 'failed';
            showNotification('文件上传失败: ' + (result.message || ''), 'error');
        }
        
        // 延迟移除进度
        setTimeout(() => {
            QQFile.uploadQueue = QQFile.uploadQueue.filter(u => u.id !== uploadId);
            QQFile.showUploadProgress();
        }, 3000);
    }).catch(err => {
        uploadItem.status = 'failed';
        showNotification('文件上传失败', 'error');
    });
};

// === 显示上传进度 ===
QQFile.showUploadProgress = function () {
    let container = $('#upload-progress-container');
    if (!container) {
        container = document.createElement('div');
        container.id = 'upload-progress-container';
        container.style.cssText = 'position:fixed;bottom:80px;right:16px;z-index:999;display:flex;flex-direction:column;gap:4px;';
        document.body.appendChild(container);
    }
    
    container.innerHTML = QQFile.uploadQueue.map(item => `
        <div class="upload-progress-item" data-id="${item.id}">
            <div class="upload-info">
                <span>${QQFile.TYPE_ICONS[item.type] || '📎'} ${escapeHtml(item.name)}</span>
                <span class="upload-status">${item.status === 'done' ? '✅' : item.status === 'failed' ? '❌' : '⏳'}</span>
            </div>
            <div class="upload-progress-bar">
                <div class="upload-progress-fill" style="width:${item.progress}%"></div>
            </div>
        </div>
    `).join('');
};

// === 图片预览 ===
QQFile.previewImage = function (url) {
    const overlay = document.createElement('div');
    overlay.className = 'dialog-overlay';
    overlay.style.background = 'rgba(0,0,0,0.85)';
    overlay.innerHTML = `
        <div style="max-width:90vw;max-height:90vh;position:relative;">
            <img src="${escapeHtml(url)}" style="max-width:100%;max-height:90vh;border-radius:8px;object-fit:contain;">
            <button class="dialog-close" style="position:absolute;top:-32px;right:0;color:white;font-size:28px;" 
                    onclick="this.closest('.dialog-overlay').remove()">&times;</button>
        </div>
    `;
    overlay.onclick = (e) => {
        if (e.target === overlay) overlay.remove();
    };
    document.body.appendChild(overlay);
};

// === 加载文件列表 ===
QQFile.loadFileList = async function (userId) {
    const result = await API.getFileList(userId);
    if (result.status === 'ok' && result.data) {
        QQFile.files = result.data.files || [];
        QQFile.renderFileList();
    }
};

// === 渲染文件列表 ===
QQFile.renderFileList = function () {
    const container = $('#file-list');
    if (!container) return;
    
    container.innerHTML = '';
    
    if (QQFile.files.length === 0) {
        container.innerHTML = '<div class="loading">暂无文件</div>';
        return;
    }
    
    QQFile.files.forEach(file => {
        const item = document.createElement('div');
        item.className = 'file-item';
        const fileType = file.file_type || QQFile.getFileType(file.file_name);
        const isImage = fileType === 'image';
        
        item.innerHTML = `
            <div class="file-icon">${QQFile.TYPE_ICONS[fileType] || '📎'}</div>
            <div class="file-info">
                <div class="file-name">${escapeHtml(file.file_name)}</div>
                <div class="file-meta">${QQFile.formatSize(file.file_size)}</div>
            </div>
            ${isImage ? `<button class="btn btn-sm btn-secondary" onclick="QQFile.previewImage('${escapeHtml(file.file_url)}')">预览</button>` : ''}
            <a href="${escapeHtml(file.file_url)}" class="btn btn-sm btn-primary" download="${escapeHtml(file.file_name)}">下载</a>
        `;
        container.appendChild(item);
    });
};

// === 初始化拖拽上传 ===
QQFile.initDragDrop = function () {
    const chatInput = $('.chat-input-area');
    if (!chatInput) return;
    
    chatInput.addEventListener('dragover', function (e) {
        e.preventDefault();
        this.style.borderColor = 'var(--color-primary)';
    });
    
    chatInput.addEventListener('dragleave', function () {
        this.style.borderColor = '';
    });
    
    chatInput.addEventListener('drop', function (e) {
        e.preventDefault();
        this.style.borderColor = '';
        
        const files = e.dataTransfer.files;
        if (files.length > 0) {
            for (let i = 0; i < files.length; i++) {
                QQFile.sendFile(files[i]);
            }
        }
    });
    
    // 文件选择按钮
    const fileBtn = document.createElement('button');
    fileBtn.type = 'button';
    fileBtn.className = 'btn btn-icon btn-file-upload';
    fileBtn.innerHTML = '📎';
    fileBtn.title = '发送文件';
    fileBtn.onclick = () => {
        const input = document.createElement('input');
        input.type = 'file';
        input.multiple = true;
        input.onchange = () => {
            for (let i = 0; i < input.files.length; i++) {
                QQFile.sendFile(input.files[i]);
            }
        };
        input.click();
    };
    
    const sendBtn = $('#btn-send');
    if (sendBtn && sendBtn.parentNode) {
        sendBtn.parentNode.insertBefore(fileBtn, sendBtn);
    }
};

// === 初始化 ===
QQFile.init = function () {
    QQFile.initDragDrop();
};

window.QQFile = QQFile;
