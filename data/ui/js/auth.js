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
        
        // 通过 IPC 保存会话到 C++ 后端（内存安全存储，不暴露给 XSS）
        IPC.send(IPC.MessageType.LOGIN, {
            user_id: result.data.id,
            token: result.data.token
        });
        
        // 仅缓存非敏感用户信息供会话恢复（无 token）
        sessionStorage.setItem('chrono_user', JSON.stringify({
            id: result.data.id,
            username: result.data.username,
            nickname: result.data.nickname
        }));
        
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
        
        // 通过 IPC 保存会话到 C++ 后端
        IPC.send(IPC.MessageType.LOGIN, {
            user_id: result.data.id,
            token: result.data.token
        });
        
        // 仅缓存非敏感用户信息
        sessionStorage.setItem('chrono_user', JSON.stringify({
            id: result.data.id,
            username: result.data.username,
            nickname: result.data.nickname
        }));
        
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
    
    sessionStorage.removeItem('chrono_user');
    sessionStorage.removeItem('chrono_token');
    
    IPC.send(IPC.MessageType.LOGOUT, {});
    
    // 切换到登录页面
    showPage('page-auth');
};

// === 恢复会话 ===
Auth.restoreSession = function () {
    // Token 只在内存中，不再持久化存储
    // 需要重新登录或通过 IPC 从 C++ 后端恢复
    const userData = sessionStorage.getItem('chrono_user');
    
    if (userData) {
        try {
            Auth.currentUser = JSON.parse(userData);
            Auth.isLoggedIn = true;
            // Token 需要重新获取（通过 IPC 或重新登录）
            API.TOKEN = null;
            return true;
        } catch (e) {
            sessionStorage.removeItem('chrono_user');
        }
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
