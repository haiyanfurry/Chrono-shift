/**
 * OAuth 登录模块 — 弹窗管理 + postMessage 监听 + 邮箱验证码
 * 依赖: api.js, utils.js, ipc.js
 */

(function () {
    'use strict';

    // ==================== OAuth 弹窗登录 ====================

    /**
     * 打开 OAuth 授权弹窗，监听 postMessage 回调
     * @param {string} provider - 'qq' 或 'wechat'
     * @returns {Promise<boolean>} 登录成功返回 true
     */
    Auth.oauthLogin = async function (provider) {
        // 1. 获取授权 URL
        const result = await API.request('GET', '/api/oauth/' + provider + '/url');
        if (result.status !== 'ok' || !result.data) {
            showNotification(result.message || '获取授权地址失败', 'error');
            return false;
        }

        var url = result.data.url;
        var state = result.data.state;

        // 2. 保存 state 用于 CSRF 验证
        var stateKey = 'oauth_state_' + provider;
        localStorage.setItem(stateKey, state);

        // 3. 打开授权弹窗
        var popup = window.open(url, '_blank', 'width=600,height=600');

        // 4. 返回 Promise，等待 postMessage 或超时
        return new Promise(function (resolve) {
            var handler = function (event) {
                // 验证消息是否来自我们的 OAuth 提供商
                if (event.data && event.data.provider === provider) {
                    window.removeEventListener('message', handler);

                    // CSRF 验证
                    var savedState = localStorage.getItem(stateKey);
                    if (event.data.state !== savedState) {
                        showNotification('安全验证失败（state 不匹配），请重试', 'error');
                        resolve(false);
                        return;
                    }
                    localStorage.removeItem(stateKey);

                    // 5. 调用后端 callback 接口交换 token
                    Auth._processOAuthCallback(provider, event.data.code, event.data.state, resolve);
                }
            };

            window.addEventListener('message', handler);

            // 6. 超时处理（2 分钟）
            setTimeout(function () {
                window.removeEventListener('message', handler);
                if (popup && !popup.closed) {
                    popup.close();
                }
                showNotification('登录超时，请重试', 'error');
                resolve(false);
            }, 120000);
        });
    };

    /**
     * 调用后端 OAuth callback 接口，处理登录结果
     * @private
     */
    Auth._processOAuthCallback = async function (provider, code, state, resolve) {
        var cb = await API.request('GET', '/api/oauth/' + provider + '/callback?code=' + encodeURIComponent(code) + '&state=' + encodeURIComponent(state));

        if (cb.status === 'ok' && cb.data) {
            // 登录成功，保存用户信息
            Auth._saveSession(cb.data);
            showNotification('登录成功', 'success');

            // 更新 UI
            var nameEl = document.getElementById('current-user-name');
            if (nameEl) {
                nameEl.textContent = cb.data.nickname || cb.data.username || '';
            }
            showPage('page-main');

            // 加载联系人
            if (typeof Contacts !== 'undefined' && Contacts.load) {
                Contacts.load();
            }

            resolve(true);
        } else {
            showNotification(cb.message || '登录失败', 'error');
            resolve(false);
        }
    };

    /**
     * 保存登录会话到 localStorage 和 IPC
     * @private
     */
    Auth._saveSession = function (data) {
        Auth.isLoggedIn = true;
        Auth.currentUser = data;
        API.TOKEN = data.token;

        // IPC 通知 C++ 后端保存会话（安全存储 token）
        if (typeof IPC !== 'undefined') {
            IPC.send(IPC.MessageType.LOGIN, {
                user_id: data.user_id,
                token: data.token
            });
        }

        // 仅保存非敏感用户信息到 sessionStorage（不持久化 token）
        var safeUser = {
            user_id: data.user_id,
            username: data.username || '',
            nickname: data.nickname || '',
            avatar_url: data.avatar_url || ''
        };
        sessionStorage.setItem('chrono_user', JSON.stringify(safeUser));
    };

    // ==================== 邮箱验证码 ====================

    /**
     * 发送邮箱验证码（60 秒冷却）
     */
    Auth.sendEmailCode = async function () {
        var emailInput = document.getElementById('reg-email');
        if (!emailInput) return;

        var email = emailInput.value.trim();
        if (!email) {
            showNotification('请先输入邮箱地址', 'error');
            return;
        }

        // 简单邮箱格式校验
        var emailRegex = /^[^\s@]+@[^\s@]+\.[^\s@]+$/;
        if (!emailRegex.test(email)) {
            showNotification('请输入有效的邮箱地址', 'error');
            return;
        }

        var result = await API.request('POST', '/api/oauth/email/send-code', { email: email });

        if (result.status === 'ok') {
            showNotification('验证码已发送到您的邮箱', 'success');
            // 启动 60 秒冷却
            var btn = document.getElementById('btn-send-code');
            if (btn) {
                btn.disabled = true;
                var sec = 60;
                btn.textContent = sec + 's';
                var timer = setInterval(function () {
                    sec--;
                    if (sec <= 0) {
                        clearInterval(timer);
                        btn.disabled = false;
                        btn.textContent = '发送验证码';
                    } else {
                        btn.textContent = sec + 's';
                    }
                }, 1000);
            }
        } else {
            showNotification(result.message || '发送失败', 'error');
        }
    };

    /**
     * 邮箱注册（验证码 + 密码）
     */
    Auth.emailRegister = async function () {
        var email = document.getElementById('reg-email');
        var code = document.getElementById('reg-email-code');
        var password = document.getElementById('reg-email-password');

        if (!email || !code || !password) return;

        var emailVal = email.value.trim();
        var codeVal = code.value.trim();
        var passwordVal = password.value;

        if (!emailVal || !codeVal || !passwordVal) {
            showNotification('请填写完整信息（邮箱、验证码、密码）', 'error');
            return false;
        }

        if (passwordVal.length < 6) {
            showNotification('密码长度不能少于 6 位', 'error');
            return false;
        }

        var result = await API.request('POST', '/api/oauth/email/register', {
            email: emailVal,
            password: passwordVal,
            code: codeVal
        });

        if (result.status === 'ok' && result.data) {
            // 注册成功，自动登录
            Auth._saveSession(result.data);
            showNotification('注册成功', 'success');

            var nameEl = document.getElementById('current-user-name');
            if (nameEl) {
                nameEl.textContent = result.data.nickname || result.data.username || '';
            }
            showPage('page-main');

            if (typeof Contacts !== 'undefined' && Contacts.load) {
                Contacts.load();
            }

            return true;
        }

        showNotification(result.message || '注册失败', 'error');
        return false;
    };

})();
