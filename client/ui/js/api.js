/**
 * Chrono-shift 网络请求 API 封装
 * 
 * 通过 IPC 或直接 HTTP 请求与服务器通信
 * Phase 2 完善具体实现
 */

const API = window.API || {};

// === 基础配置 ===
API.BASE_URL = 'http://127.0.0.1:8080';
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
API.getProfile = function () {
    return API.request('GET', '/api/user/profile');
};

// 更新用户信息
API.updateProfile = function (nickname, avatar_url) {
    return API.request('PUT', '/api/user/update', {
        nickname, avatar_url
    });
};

// 搜索用户
API.searchUsers = function (keyword) {
    return API.request('GET', `/api/user/search?keyword=${encodeURIComponent(keyword)}`);
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

window.API = API;
