/**
 * Chrono-shift 网络请求 API 封装
 * 
 * 通过 IPC 或直接 HTTP 请求与服务器通信
 * Phase 2 完善具体实现
 */

const API = window.API || {};

// === 基础配置 ===
API.BASE_URL = 'https://127.0.0.1:4443';
API.TOKEN = null;

// === HTTP 请求 ===
API.request = async function (method, path, data) {
    const url = `${API.BASE_URL}${path}`;
    const headers = {
        'Content-Type': 'application/json'
    };
    
    if (API.TOKEN) {
        headers['Authorization'] = `Bearer ${API.TOKEN}`;
    }
    
    try {
        const response = await fetch(url, {
            method: method,
            headers: headers,
            body: data ? JSON.stringify(data) : undefined
        });
        
        const result = await response.json();
        return result;
    } catch (error) {
        console.error('[API Error]', method, path, error);
        return { status: 'error', message: '网络连接失败' };
    }
};

// === API 接口 ===

// 用户注册
API.register = function (username, password, nickname) {
    return API.request('POST', '/api/user/register', {
        username, password, nickname
    });
};

// 用户登录
API.login = function (username, password) {
    return API.request('POST', '/api/user/login', {
        username, password
    });
};

// 获取用户信息
API.getProfile = function (userId) {
    const query = userId ? `?user_id=${userId}` : '';
    return API.request('GET', `/api/user/profile${query}`);
};

// 更新用户信息
API.updateProfile = function (nickname, avatar_url) {
    const data = { nickname, avatar_url };
    // 如果有当前用户 ID，一并发送
    if (window.Auth && window.Auth.currentUser && window.Auth.currentUser.id) {
        data.user_id = window.Auth.currentUser.id;
    }
    return API.request('PUT', '/api/user/update', data);
};

// 搜索用户
API.searchUsers = function (keyword) {
    return API.request('GET', `/api/user/search?q=${encodeURIComponent(keyword)}`);
};

// 获取好友列表
API.getFriends = function () {
    return API.request('GET', '/api/user/friends');
};

// 添加好友
API.addFriend = function (friendId) {
    return API.request('POST', '/api/user/friends/add', {
        friend_id: friendId
    });
};

// 发送消息
API.sendMessage = function (toUserId, content) {
    return API.request('POST', '/api/message/send', {
        to_user_id: toUserId,
        content: content
    });
};

// 获取消息历史
API.getMessages = function (userId, offset = 0, limit = 50) {
    return API.request('GET', `/api/message/list?user_id=${userId}&offset=${offset}&limit=${limit}`);
};

// 获取模板列表
API.getTemplates = function (offset = 0, limit = 20) {
    return API.request('GET', `/api/templates?offset=${offset}&limit=${limit}`);
};

// 应用模板
API.applyTemplate = function (templateId) {
    return API.request('POST', '/api/templates/apply', {
        template_id: templateId
    });
};

// === 好友管理 API（QQ 风格） ===

// 获取好友分组
API.getFriendGroups = function () {
    return API.request('GET', '/api/friend/groups');
};

// 创建好友分组
API.createFriendGroup = function (data) {
    return API.request('POST', '/api/friend/groups/create', data);
};

// 重命名分组
API.renameFriendGroup = function (groupId, name) {
    return API.request('PUT', `/api/friend/groups/${groupId}`, { name });
};

// 删除分组
API.deleteFriendGroup = function (groupId) {
    return API.request('DELETE', `/api/friend/groups/${groupId}`);
};

// 移动好友至分组
API.moveFriendToGroup = function (userId, groupId) {
    return API.request('PUT', `/api/friend/move`, {
        user_id: userId,
        group_id: groupId
    });
};

// 设置好友备注
API.updateFriendNote = function (userId, note) {
    return API.request('PUT', `/api/friend/note`, {
        user_id: userId,
        note: note
    });
};

// 发送好友申请（带验证消息）
API.addFriendRequest = function (userId, data) {
    return API.request('POST', `/api/friend/request`, {
        user_id: userId,
        message: data.message || '你好，加个好友吧！'
    });
};

// 获取待处理的好友申请
API.getFriendRequests = function () {
    return API.request('GET', '/api/friend/requests');
};

// 同意好友申请
API.approveFriendRequest = function (requestId) {
    return API.request('POST', `/api/friend/request/${requestId}/approve`);
};

// 拒绝好友申请
API.rejectFriendRequest = function (requestId) {
    return API.request('POST', `/api/friend/request/${requestId}/reject`);
};

// 拉黑用户
API.blockUser = function (userId) {
    return API.request('POST', '/api/friend/block', { user_id: userId });
};

// 取消拉黑
API.unblockUser = function (userId) {
    return API.request('POST', '/api/friend/unblock', { user_id: userId });
};

// 获取黑名单
API.getBlacklist = function () {
    return API.request('GET', '/api/friend/blacklist');
};

// 获取最近联系人
API.getRecentContacts = function () {
    return API.request('GET', '/api/friend/recent');
};

// === 群组 API ===

// 创建群组
API.createGroup = function (data) {
    return API.request('POST', '/api/group/create', data);
};

// 获取群组列表
API.getGroupList = function () {
    return API.request('GET', '/api/group/list');
};

// 获取群组详情
API.getGroupDetail = function (groupId) {
    return API.request('GET', `/api/group/${groupId}`);
};

// 加入群组
API.joinGroup = function (groupId) {
    return API.request('POST', `/api/group/${groupId}/join`);
};

// 退出群组
API.leaveGroup = function (groupId) {
    return API.request('POST', `/api/group/${groupId}/leave`);
};

// 获取群组成员
API.getGroupMembers = function (groupId) {
    return API.request('GET', `/api/group/${groupId}/members`);
};

// 发送群消息
API.sendGroupMessage = function (groupId, content) {
    return API.request('POST', '/api/group/message/send', {
        group_id: groupId,
        content: content
    });
};

// 获取群消息
API.getGroupMessages = function (groupId, offset, limit) {
    return API.request('GET', `/api/group/${groupId}/messages?offset=${offset || 0}&limit=${limit || 50}`);
};

// === 文件分享 API ===

// 上传文件
API.uploadFile = function (formData) {
    const url = `${API.BASE_URL}/api/file/upload`;
    const headers = {};
    if (API.TOKEN) {
        headers['Authorization'] = `Bearer ${API.TOKEN}`;
    }
    return fetch(url, {
        method: 'POST',
        headers: headers,
        body: formData
    }).then(r => r.json());
};

// 获取文件列表
API.getFileList = function (userId) {
    const query = userId ? `?user_id=${userId}` : '';
    return API.request('GET', `/api/file/list${query}`);
};

// 删除文件
API.deleteFile = function (fileId) {
    return API.request('DELETE', `/api/file/${fileId}`);
};

// === 状态签名 API ===

// 更新在线状态
API.updateStatus = function (status, signature) {
    return API.request('PUT', '/api/user/status', { status, signature });
};

// 获取用户状态
API.getUserStatus = function (userId) {
    return API.request('GET', `/api/user/${userId}/status`);
};

// 上传头像
API.uploadAvatar = function (formData) {
    const url = `${API.BASE_URL}/api/user/avatar`;
    const headers = {};
    if (API.TOKEN) {
        headers['Authorization'] = `Bearer ${API.TOKEN}`;
    }
    return fetch(url, {
        method: 'POST',
        headers: headers,
        body: formData
    }).then(r => r.json());
};

window.API = API;
