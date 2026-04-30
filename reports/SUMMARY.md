# 墨竹 (Chrono-shift) 综合测试报告

> **测试时间**: 2026年4月  
> **测试环境**: Windows 11, 本地服务器  
> **项目版本**: v0.1.0

---

## 1. 安全渗透测试

**文件**: [`tests/security_pen_test.sh`](../tests/security_pen_test.sh)

| 测试类别 | 覆盖内容 | 预期结果 |
|---------|---------|---------|
| SQL 注入 | OR 1=1, UNION SELECT, DROP TABLE | 服务端正确转义/拒绝恶意输入 |
| XSS 攻击 | `<script>` 标签, `<img onerror>` | 输入被 HTML 转义, 不执行脚本 |
| 路径遍历 | `../../../etc/passwd`, `..\..\` | 路径被规范化/拒绝 |
| JWT 伪造 | alg:none, 过期 token, 随机字符串 | 服务端验证签名, 拒绝无效 token |
| 权限绕过 | 未授权访问受保护接口 | 返回 401/403 |
| 大负载测试 | 1000 字符用户名, 10000 字符消息 | 服务端正确处理/截断 |
| 输入验证 | 空值, 特殊字符, 超长输入 | 服务端正确校验 |

**执行命令**:
```bash
bash tests/security_pen_test.sh
```

---

## 2. API 接口验证

**文件**: [`tests/api_verification_test.sh`](../tests/api_verification_test.sh)

| 接口 | 方法 | 路径 | 状态 |
|------|------|------|------|
| 健康检查 | GET | `/api/health` | ✅ 已覆盖 |
| 用户注册 | POST | `/api/user/register` | ✅ 已覆盖 |
| 用户登录 | POST | `/api/user/login` | ✅ 已覆盖 |
| 获取资料 | GET | `/api/user/profile` | ✅ 已覆盖 |
| 更新资料 | POST | `/api/user/update` | ✅ 已覆盖 |
| 搜索用户 | GET | `/api/user/search` | ✅ 已覆盖 |
| 获取好友 | GET | `/api/user/friends` | ✅ 已覆盖 |
| 发送消息 | POST | `/api/message/send` | ✅ 已覆盖 |
| 获取消息 | GET | `/api/messages` | ✅ 已覆盖 |
| 获取模板 | GET | `/api/templates` | ✅ 已覆盖 |
| 应用模板 | POST | `/api/templates/apply` | ✅ 已覆盖 |
| IPC 消息类型 | — | 0x01–0xFF | ✅ 9 种类型全覆盖 |

**执行命令**:
```bash
bash tests/api_verification_test.sh
```

---

## 3. CLI 调试工具扩展

**文件**: [`server/tools/debug_cli.c`](../server/tools/debug_cli.c)

| 命令 | 功能 | 状态 |
|------|------|------|
| `help` | 显示帮助信息 | ✅ |
| `ipc types` | 列出所有 IPC 消息类型 | ✅ (10 种, 含 OPEN_URL 0x50) |
| `ipc send <hex> <json>` | 发送 IPC 消息 | ✅ |
| `user` | 用户管理 | ✅ |
| `exit/quit` | 退出 | ✅ |

**编译命令**:
```bash
cd server && make debug-cli
```

**使用示例**:
```
./build/debug_cli
> ipc types
> ipc send 0x01 {"username":"test"}
> user register test pass123
```

---

## 4. UI 美化改造

| 文件 | 变更内容 |
|------|---------|
| [`variables.css`](../client/ui/css/variables.css) | 靛蓝色系 + 毛玻璃 + 渐变 + 阴影系统 + 不对称气泡 |
| [`global.css`](../client/ui/css/global.css) | 按钮系统增强 + 表单焦点动画 + 徽标 + 空状态 |
| [`login.css`](../client/ui/css/login.css) | 全屏渐变背景 + 毛玻璃卡片 + 装饰浮动圆 + 动画 |
| [`main.css`](../client/ui/css/main.css) | 侧边栏渐变背景 + 外部链接区域 + 底部导航图标 |
| [`chat.css`](../client/ui/css/chat.css) | 不对称气泡圆角 + 气泡阴影 + 日期分隔线 |
| [`community.css`](../client/ui/css/community.css) | 卡片悬浮上移效果 + 毛玻璃卡片 + 设置卡片 |
| [`themes/default.css`](../client/ui/css/themes/default.css) | 简化为主题微调文件 |
| [`index.html`](../client/ui/index.html) | 登录装饰元素 + 侧边栏外部链接 + 状态指示点 |
| [`app.js`](../client/ui/js/app.js) | `openExternalUrl()` 函数 + 链接配置 |

**设计风格**: 现代简约 · 靛蓝主色调 · 毛玻璃效果 · 柔和渐变 · 不对称气泡

---

## 5. 外部网站跳转入口

| 网站 | URL | 触发方式 |
|------|-----|---------|
| Bilibili | <https://www.bilibili.com> | IPC_OPEN_URL + window.open |
| AcFun | <https://www.acfun.cn> | IPC_OPEN_URL + window.open |
| CP 漫展官网 | <https://www.comic-expo.com> | IPC_OPEN_URL + window.open |

**IPC 消息类型**: `IPC_OPEN_URL = 0x50`  
**协议**: [`ipc_bridge.h`](../client/include/ipc_bridge.h) + [`ipc.js`](../client/ui/js/ipc.js)

侧边栏外部链接区域位于联系人列表与底部导航之间。

---

## 6. 压力测试框架

**文件**: [`server/tools/stress_test.c`](../server/tools/stress_test.c)

| 功能 | 描述 |
|------|------|
| 并发模型 | 多线程 HTTP 请求 (最多 64 线程) |
| QPS 控制 | 精确到微秒的速率限制 |
| 统计指标 | QPS, P50/P90/P95/P99 延迟, 错误率, 成功率 |
| 测试场景 | 健康检查, 注册, 登录, 获取模板, 发送消息 |
| 报告生成 | 自动保存 Markdown 报告到 `reports/` |
| 抗冲击评估 | 基于错误率的自动等级评定 |
| 平台支持 | Windows (WinSock2) + Linux (POSIX) |

**编译命令**:
```bash
cd server && make stress-test
```

**使用示例**:
```bash
./build/stress_test --host 127.0.0.1 --port 8080 --threads 8 --qps 200 --duration 60
./build/stress_test --list-scenarios
./build/stress_test --scenario 2 --qps 500 --threads 16 --duration 120
```

---

## 7. 回环测试 (Loopback Test)

**文件**: [`tests/loopback_test.sh`](../tests/loopback_test.sh)

| 步骤 | 测试内容 | 描述 |
|------|---------|------|
| 1 | 服务器可访问性 | 自动检测/启动服务器 |
| 2 | 健康检查 | GET `/api/health` → 200 |
| 3 | 用户注册 | POST 注册 → 获取 user_id |
| 4 | 用户登录 | POST 登录 → 获取 JWT Token |
| 5 | 获取资料 | GET profile → 验证昵称 |
| 6 | 更新资料 | POST update → 修改昵称 |
| 7 | 发送消息 | POST message → 获取 message_id |
| 8 | 获取历史 | GET messages → 验证消息数 |
| 9 | 获取模板 | GET templates → 验证模板数 |
| 10 | 搜索用户 | GET search → 验证搜索结果 |
| 11 | CLI 验证 | 验证 debug_cli 的 IPC 命令 |

**执行命令**:
```bash
bash tests/loopback_test.sh
```

---

## 8. 清理脚本

### Windows (`cleanup.bat`)
```batch
cleanup.bat          # 清理所有
cleanup.bat --all    # 清理所有
cleanup.bat --test   # 仅清理测试文件
cleanup.bat --build  # 仅清理构建产物
cleanup.bat --logs   # 仅清理日志
cleanup.bat --help   # 显示帮助
```

### Linux/macOS (`cleanup.sh`)
```bash
chmod +x cleanup.sh
./cleanup.sh --all
./cleanup.sh --build --logs
./cleanup.sh --help
```

---

## 操作流程

### 完整执行流程

```bash
# 1. 编译服务器
cd server && make build

# 2. 编译调试 CLI
make debug-cli

# 3. 编译压力测试工具
make stress-test

# 4. 启动服务器 (另一个终端)
./build/chrono-server

# 5. 运行安全渗透测试
bash ../tests/security_pen_test.sh

# 6. 运行 API 接口验证
bash ../tests/api_verification_test.sh

# 7. 运行回环测试
bash ../tests/loopback_test.sh

# 8. 运行压力测试
./build/stress_test --threads 8 --qps 200 --duration 60

# 9. 清理
cd .. && cleanup.bat --all
```

---

## 文件清单

| 文件 | 类型 | 用途 |
|------|------|------|
| `tests/security_pen_test.sh` | 新增 | 安全渗透测试 |
| `tests/api_verification_test.sh` | 新增 | API 接口验证 |
| `server/tools/debug_cli.c` | 修改 | CLI 调试工具 (IPC 扩展) |
| `server/tools/stress_test.c` | 新增 | 压力测试框架 |
| `tests/loopback_test.sh` | 新增 | 回环测试 |
| `cleanup.bat` | 新增 | Windows 清理脚本 |
| `cleanup.sh` | 新增 | Linux 清理脚本 |
| `client/ui/css/variables.css` | 修改 | CSS 变量系统 |
| `client/ui/css/global.css` | 修改 | 全局样式 |
| `client/ui/css/login.css` | 修改 | 登录页样式 |
| `client/ui/css/main.css` | 修改 | 主布局样式 |
| `client/ui/css/chat.css` | 修改 | 聊天样式 |
| `client/ui/css/community.css` | 修改 | 社区样式 |
| `client/ui/css/themes/default.css` | 修改 | 默认主题 |
| `client/ui/index.html` | 修改 | 主 HTML |
| `client/ui/js/app.js` | 修改 | 应用入口 |
| `client/include/ipc_bridge.h` | 修改 | IPC 类型枚举 |
| `client/ui/js/ipc.js` | 修改 | IPC JS 接口 |
| `server/Makefile` | 修改 | 编译目标 |
| `reports/SUMMARY.md` | 新增 | 本综合报告 |
| `reports/security_pen_test_results.md` | 自动生成 | 渗透测试报告 |
| `reports/api_verification_results.md` | 自动生成 | API 验证报告 |
| `reports/stress_test_report.md` | 自动生成 | 压力测试报告 |
| `reports/loopback_test_report.md` | 自动生成 | 回环测试报告 |
