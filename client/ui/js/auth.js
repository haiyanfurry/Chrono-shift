/**
 * Chrono-shift 认证管理
 * 处理登录、注册、令牌管理、OAuth 登录、邮箱注册
 */

const Auth = window.Auth || {};

// === 状态 ===
Auth.isLoggedIn = false;
Auth.currentUser = null;

// === 登录 ===
Auth.login = async function (username, password) {
    const result = await API.login(username, password);
    
    if (result.status === 'ok' && result.data) {
        Auth.isLoggedIn = true;
        Auth.currentUser = result.data;
        API.TOKEN = result.data.token;
        
        // 保存会话
        IPC.send(IPC.MessageType.LOGIN, {
            user_id: result.data.id,
            token: result.data.token
        });
        
        // 保存令牌到本地存储
        localStorage.setItem('chrono_token', result.data.token);
        localStorage.setItem('chrono_user', JSON.stringify(result.data));
        
        return true;
    }
    
    showNotification(result.message || '登录失败', 'error');
    return false;
};

// === 注册 (服务端直接返回 token，注册即登录) ===
Auth.register = async function (username, password, nickname) {
    const result = await API.register(username, password, nickname);
    
    if (result.status === 'ok' && result.data) {
        // 注册成功，服务端已返回 token，自动登录
        Auth.isLoggedIn = true;
        Auth.currentUser = result.data;
        API.TOKEN = result.data.token;
        
        // 保存会话
        IPC.send(IPC.MessageType.LOGIN, {
            user_id: result.data.id,
            token: result.data.token
        });
        
        // 保存令牌到本地存储
        localStorage.setItem('chrono_token', result.data.token);
        localStorage.setItem('chrono_user', JSON.stringify(result.data));
        
        showNotification('注册成功', 'success');
        return true;
    }
    
    showNotification(result.message || '注册失败', 'error');
    return false;
};

// === 退出登录 ===
Auth.logout = function () {
    Auth.isLoggedIn = false;
    Auth.currentUser = null;
    API.TOKEN = null;
    
    localStorage.removeItem('chrono_token');
    localStorage.removeItem('chrono_user');
    
    IPC.send(IPC.MessageType.LOGOUT, {});
    
    // 切换到登录页面
    showPage('page-auth');
};

// === 恢复会话 ===
Auth.restoreSession = function () {
    const token = localStorage.getItem('chrono_token');
    const userData = localStorage.getItem('chrono_user');
    
    if (token && userData) {
        API.TOKEN = token;
        Auth.currentUser = JSON.parse(userData);
        Auth.isLoggedIn = true;
        return true;
    }
    
    return false;
};

// === OAuth 登录（委托给 oauth.js） ===

/**
 * QQ 登录 — 打开 OAuth 弹窗
 */
Auth.qqLogin = async function () {
    return await Auth.oauthLogin('qq');
};

/**
 * 微信登录 — 打开 OAuth 弹窗
 */
Auth.wechatLogin = async function () {
    return await Auth.oauthLogin('wechat');
};

/**
 * 发送邮箱验证码
 */
Auth.sendEmailCode = async function () {
    return await Auth.sendEmailCode();
};

/**
 * 邮箱注册（验证码 + 密码）
 */
Auth.emailRegister = async function () {
    return await Auth.emailRegister();
};

window.Auth = Auth;
