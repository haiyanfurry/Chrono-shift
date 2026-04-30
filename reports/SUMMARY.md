# 墨竹 (Chrono-shift) 综合测试报告

> **测试时间**: 2026年4月  
> **测试环境**: Windows 11, 本地服务器  
> **项目版本**: v0.2.0

---

## 1. 安全渗透测试

**文件**: [`tests/security_pen_test.sh`](../tests/security_pen_test.sh)

### 测试类别

| # | 类别 | 子测试 | 预期结果 |
|---|------|--------|---------|
| 1 | SQL 注入 | OR 1=1, UNION SELECT, DROP TABLE | 服务端正确转义/拒绝恶意输入 |
| 2 | XSS 攻击 | `<script>` 标签, `<img onerror>` | 输入被 HTML 转义, 不执行脚本 |
| 3 | 路径遍历 | `../../../etc/passwd`, `..\..\` | 路径被规范化/拒绝 |
| 4 | JWT 伪造 | alg:none, 过期 token, 随机字符串 | 服务端验证签名, 拒绝无效 token |
| 5 | 权限绕过 | 未授权访问受保护接口 | 返回 401/403 |
| 6 | 大负载测试 | 1000 字符用户名, 10000 字符消息 | 服务端正确处理/截断 |
| 7 | 输入验证 | 空值, 特殊字符, 超长输入 | 服务端正确校验 |
| **8** | **CSRF 防护** | 缺失 Origin 头/跨域请求/空 CSRF Token/无效 Token/text/plain 绕过 | **新增** 返回 401-403 |
| **9** | **SSRF 防护** | 127.0.0.1/10.0.0.1/169.254.169.254/file:// 协议/URL 重定向/DNS rebinding | **新增** 请求被拒绝 |
| **10** | **HTTPS 降级** | HTTP 明文访问/TLS 1.0 强制降级 | **新增** 连接被拒绝 |

### 执行命令

```bash
bash tests/security_pen_test.sh
```

---

## 2. API 接口验证

**文件**: [`tests/api_verification_test.sh`](../tests/api_verification_test.sh)

| # | 接口 | 方法 | 路径 | 状态 |
|---|------|------|------|------|
| 1 | 健康检查 | GET | `/api/health` | ✅ |
| 2 | 用户注册 | POST | `/api/user/register` | ✅ |
| 3 | 用户登录 | POST | `/api/user/login` | ✅ |
| 4 | 获取资料 | GET | `/api/user/profile` | ✅ |
| 5 | 更新资料 | PUT | `/api/user/update` | ✅ |
| 6 | 搜索用户 | GET | `/api/user/search` | ✅ |
| 7 | 获取好友 | GET | `/api/user/friends` | ✅ |
| 8 | 发送消息 | POST | `/api/message/send` | ✅ |
| 9 | 获取消息 | GET | `/api/message/list` | ✅ |
| 10 | 获取模板 | GET | `/api/templates` | ✅ |
| 11 | 应用模板 | POST | `/api/templates/apply` | ✅ |
| **2.6** | **文件系统** | POST/GET | 上传/列表/下载/头像/未授权 | **新增** |
| **2.7** | **好友系统** | POST/GET | 注册第二用户/添加好友/好友列表 | **新增** |
| **2.8** | **模板 CRUD** | POST/GET/PUT/DELETE | 创建/列表/更新/应用/删除模板 | **新增** |

**处理器模块映射**:

| 模块 | 源文件 | 行数 | 覆盖接口 |
|------|--------|------|---------|
| 用户系统 | [`user_handler.c`](../server/src/user_handler.c) | 405 | 7 个接口 |
| 消息服务 | [`message_handler.c`](../server/src/message_handler.c) | 618 | 2 个接口 + WebSocket |
| 社区模板 | [`community_handler.c`](../server/src/community_handler.c) | 450 | 4 个接口 |
| 文件服务 | [`file_handler.c`](../server/src/file_handler.c) | 738 | 3 个接口 |

---

## 3. CLI 调试工具扩展

**文件**: [`server/tools/debug_cli.c`](../server/tools/debug_cli.c) (2317 行)

| 命令 | 子命令 | 功能 | 状态 |
|------|--------|------|------|
| `help` | — | 显示所有可用命令（含 E1/E2 帮助） | ✅ 已增强 |
| `ipc types` | — | 列出所有 IPC 消息类型 (10 种) | ✅ |
| `ipc send` | `<hex> <json>` | 发送 IPC 消息 | ✅ |
| `user` | `register/login/profile/search` | 用户管理 | ✅ |
| **`ws`** | **`connect/send/recv/close/status`** | **WebSocket 调试 (E1)** | **新增** |
| **`msg`** | **`list/get/send`** | **数据库消息操作 (E2)** | **新增** |
| **`friend`** | **`list/add`** | **好友管理 (E2)** | **新增** |
| **`db`** | **`list`** | **数据库内容浏览 (E2)** | **新增** |
| `exit/quit` | — | 退出 | ✅ |

### WebSocket 调试 (E1) 实现详情

- **SHA-1 实现**: 纯 C 实现 (不依赖 OpenSSL)，用于 WebSocket 握手 Accept Key 计算
- **Base64 编码**: 自定义 Base64 实现
- **帧编解码**: 支持 FIN/opcode/mask/payload 的标准 RFC 6455 帧格式
- **非阻塞 I/O**: `ws recv` 使用非阻塞模式

### 数据库操作 (E2) 实现详情

- `msg list` — 通过 `db_get_messages()` 接口列出用户消息
- `msg send` — 通过 `db_send_message()` 直接写入数据库
- `friend list/add` — 通过 `db_get_friends()` / `db_add_friend()` 接口
- `db list` — 支持 `users` / `messages` / `friends` / `templates` 四种类型

---

## 4. UI QQ 风格美化改造

**设计风格**: **QQ 风格** · 纯白背景 · `#12B7F5` 蓝色主色调 · `#9EEA6A` 自聊气泡 · 毛玻璃效果 · 不对称气泡

| 文件 | 变更内容 |
|------|---------|
| [`variables.css`](../client/ui/css/variables.css) | QQ 风格色系: `--primary: #12B7F5`, `--self-bubble: #9EEA6A`, `--bg-primary: #FFFFFF` |
| [`login.css`](../client/ui/css/login.css) | 浅蓝渐变背景 + 白色毛玻璃卡片 + 居中简洁布局 |
| [`main.css`](../client/ui/css/main.css) | 280px 固定侧边栏 + 底部蓝色导航指示条 + 左侧蓝色联系人指示条 |
| [`chat.css`](../client/ui/css/chat.css) | 绿色自气泡 (`#9EEA6A`) + 白色对方气泡 (`#FFFFFF` 带 `#E0E0E0` 边框) + 不对称圆角 |
| [`community.css`](../client/ui/css/community.css) | 简化卡片风格 + 更浅阴影 `rgba(0,0,0,0.06)` |
| [`global.css`](../client/ui/css/global.css) | 添加 `badge-dot` 消息红点类 + 扁平化按钮样式 |
| [`themes/default.css`](../client/ui/css/themes/default.css) | 纯白默认主题微调 |
| [`index.html`](../client/ui/index.html) | 标题改为 "QQ 风格社交平台" + 侧边栏外部链接区域 + 状态指示点 |

### 核心 CSS 颜色变量

```css
:root {
  --primary: #12B7F5;        /* QQ 蓝 */
  --primary-hover: #10A5DE;   /* 深蓝 hover */
  --self-bubble: #9EEA6A;     /* 自聊绿气泡 */
  --other-bubble: #FFFFFF;    /* 对方白气泡 */
  --bg-primary: #FFFFFF;      /* 纯白背景 */
  --sidebar-bg: #F8F9FA;     /* 侧边栏浅灰 */
  --text-primary: #333333;    /* 深灰文字 */
  --text-secondary: #999999;  /* 浅灰辅助文字 */
  --border-color: #E8E8E8;   /* 边框色 */
}
```

---

## 5. 外部网站跳转入口

| 网站 | URL | 触发方式 |
|------|-----|---------|
| Bilibili | <https://www.bilibili.com> | IPC_OPEN_URL + window.open |
| AcFun | <https://www.acfun.cn> | IPC_OPEN_URL + window.open |
| CP 漫展官网 | <https://www.comic-expo.com> | IPC_OPEN_URL + window.open |

**IPC 消息类型**: `IPC_OPEN_URL = 0x50`  
**协议文件**: [`ipc_bridge.h`](../client/include/ipc_bridge.h) + [`ipc.js`](../client/ui/js/ipc.js)

---

## 6. 压力测试框架

**文件**: [`server/tools/stress_test.c`](../server/tools/stress_test.c) (776 行)

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
out/stress_test --host 127.0.0.1 --port 4443 --threads 8 --qps 200 --duration 60
out/stress_test --list-scenarios
out/stress_test --scenario 2 --qps 500 --threads 16 --duration 120
```

---

## 7. 回环测试 (Loopback Test)

**文件**: [`tests/loopback_test.sh`](../tests/loopback_test.sh)

| 步骤 | 测试内容 | 描述 |
|------|---------|------|
| 1 | 服务器可访问性 | 自动检测/启动/等待服务器 |
| 2 | 健康检查 | GET `/api/health` → 200 |
| 3 | 用户注册 | POST 注册 → 获取 user_id |
| 4 | 用户登录 | POST 登录 → 获取 JWT Token |
| 5 | 获取资料 | GET profile → 验证昵称 |
| 6 | 更新资料 | PUT update → 修改昵称 |
| 7 | 发送消息 | POST message → 获取 message_id |
| 8 | 获取历史 | GET messages → 验证消息数 |
| 9 | 获取模板 | GET templates → 验证模板数 |
| 10 | 搜索用户 | GET search → 验证搜索结果 |
| 11 | CLI 验证 | 验证 debug_cli 的 IPC 命令 |
| **12** | **文件上传下载** | **POST 上传文件 → GET 下载验证** | **新增** |
| **13** | **模板 CRUD** | **创建模板 → 应用模板 → 列表验证** | **新增** |
| **14** | **好友系统** | **添加好友 → 好友列表验证** | **新增** |

---

## 8. 编译验证

### 最终编译验证结果

所有 **17 个 server 源文件** 零错误通过编译:

| # | 源文件 | 行数 | 编译状态 |
|---|--------|------|---------|
| 1 | [`main.c`](../server/src/main.c) | 189 | ✅ 通过 |
| 2 | [`http_server.c`](../server/src/http_server.c) | 911 | ✅ 通过 |
| 3 | [`websocket.c`](../server/src/websocket.c) | 463 | ✅ 通过 |
| 4 | [`database.c`](../server/src/database.c) | 1214 | ✅ 通过 |
| 5 | [`user_handler.c`](../server/src/user_handler.c) | 405 | ✅ 通过 |
| 6 | [`message_handler.c`](../server/src/message_handler.c) | 618 | ✅ 通过 |
| 7 | [`community_handler.c`](../server/src/community_handler.c) | 450 | ✅ 通过 |
| 8 | [`file_handler.c`](../server/src/file_handler.c) | 738 | ✅ 通过 |
| 9 | [`json_parser.c`](../server/src/json_parser.c) | 642 | ✅ 通过 |
| 10 | [`protocol.c`](../server/src/protocol.c) | 53 | ✅ 通过 |
| 11 | [`utils.c`](../server/src/utils.c) | 149 | ✅ 通过 |
| 12 | [`tls_server.c`](../server/src/tls_server.c) | 384 | ✅ 通过 |
| 13 | [`tls_stub.c`](../server/src/tls_stub.c) | 42 | ✅ 通过 |
| 14 | [`rust_stubs.c`](../server/src/rust_stubs.c) | 104 | ✅ 通过 |
| 15 | [`debug_cli.c`](../server/tools/debug_cli.c) | 2317 | ✅ 通过 |
| 16 | [`stress_test.c`](../server/tools/stress_test.c) | 776 | ✅ 通过 |
| 17 | [`client_http_server.c`](../client/src/client_http_server.c) | — | ✅ 通过 |

**编译命令**: `gcc -std=c99 -Wall -Wextra -Iinclude -c <file>.c`

> 仅保留预存警告 (platform_compat.h 类型转换、#pragma 未知、未使用函数、snprintf 大小提示)

---

## 9. 文件变更清单

| 文件 | 类型 | 变更 | 用途 |
|------|------|------|------|
| `server/src/message_handler.c` | **新增** | 618 行 | HTTP/WS 消息系统 |
| `server/src/community_handler.c` | **新增** | 450 行 | 模板 CRUD |
| `server/src/file_handler.c` | **新增** | 738 行 | 文件上传/下载/头像 |
| `server/tools/debug_cli.c` | **修改** | 2317 行 | 增强 ws/msg/friend/db 命令 |
| `tests/security_pen_test.sh` | **修改** | +13 测试 | CSRF/SSRF/HTTPS降级 |
| `tests/api_verification_test.sh` | **修改** | +3 节 | 文件/好友/模板 CRUD |
| `tests/loopback_test.sh` | **修改** | +3 步 | 文件/模板/好友全链路 |
| `client/ui/css/variables.css` | **修改** | QQ 风格 | #12B7F5/#9EEA6A 色系 |
| `client/ui/css/login.css` | **修改** | QQ 风格 | 浅蓝渐变白卡 |
| `client/ui/css/main.css` | **修改** | QQ 风格 | 280px 侧边栏 |
| `client/ui/css/chat.css` | **修改** | QQ 风格 | 绿/白不对称气泡 |
| `client/ui/css/community.css` | **修改** | QQ 风格 | 简化卡片 |
| `client/ui/css/global.css` | **修改** | QQ 风格 | badge-dot 红点 |
| `client/ui/css/themes/default.css` | **修改** | QQ 风格 | 纯白默认主题 |
| `client/ui/index.html` | **修改** | QQ 风格 | 标题/外部链接 |
| `server/tools/stress_test.c` | 已存在 | 776 行 | 压力测试框架 |
| `cleanup.bat` | 已存在 | — | Windows 清理 |
| `cleanup.sh` | 已存在 | — | Linux 清理 |

---

## 10. 完整操作流程

```bash
# 1. 编译服务器
cd server && mingw32-make build

# 2. 编译调试 CLI
mingw32-make debug-cli

# 3. 编译压力测试工具
mingw32-make stress-test

# 4. 启动服务器 (另一终端)
set PATH=D:\mys32\mingw64\bin;%PATH%
out\chrono-server.exe --port 4443

# 5. 运行安全渗透测试 (10 类, 30+ 测试)
bash ../tests/security_pen_test.sh

# 6. 运行 API 接口验证 (含文件/好友/模板 CRUD)
bash ../tests/api_verification_test.sh

# 7. 运行端到端回环测试 (14 步全链路)
bash ../tests/loopback_test.sh

# 8. 运行压力测试
out/stress_test --threads 8 --qps 200 --duration 60

# 9. 清理
cd .. && cleanup.bat --all
```
