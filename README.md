# Chrono-shift (墨竹) — QQ 风格社交平台

> 一个轻量级即时通讯与社区平台 — QQ 风格 UI、跨平台、模块化、可扩展

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)
![Language: C99 + Rust + JavaScript](https://img.shields.io/badge/language-C99%20%7C%20Rust%20%7C%20JavaScript-blue)
![Platform: Windows + Linux](https://img.shields.io/badge/platform-Windows%20%7C%20Linux-lightgrey)

---

## 目录

- [项目概述](#项目概述)
- [技术架构](#技术架构)
- [功能特性](#功能特性)
- [快速开始](#快速开始)
  - [环境要求](#环境要求)
  - [编译构建](#编译构建)
  - [运行服务器](#运行服务器)
- [项目结构](#项目结构)
- [API 文档](#api-文档)
- [IPC 协议](#ipc-协议)
- [测试](#测试)
- [安装包制作](#安装包制作)
- [开源协议](#开源协议)
- [联系方式](#联系方式)

---

## 项目概述

**Chrono-shift (墨竹)** 是一款使用纯 C99 (服务端) + Web 技术 (客户端) 构建的即时通讯平台，采用 **QQ 风格用户界面**，支持用户注册登录、消息收发、社区互动、文件传输、模板自定义等功能。

后端采用 epoll/poll 事件驱动架构，前端为 **QQ 风格 Web UI**（主色调 `#12B7F5` 蓝色，自聊气泡 `#9EEA6A` 绿色），安全模块使用 Rust 编写 (Argon2id 密码哈希 + JWT 认证)。

### 设计目标

- **轻量**: 纯 C 编写，低内存占用 (≈ 10MB)
- **跨平台**: Windows (WSAPoll) / Linux (epoll)
- **安全**: Argon2id 密码哈希 + JWT 令牌认证 + CSRF/SSRF 防护
- **可扩展**: 模块化路由设计（用户/消息/社区/文件），IPC 消息接口
- **QQ 风格**: 默认纯白背景，蓝色主色调，绿色自聊气泡，280px 侧边栏

---

## 技术架构

```
┌─────────────────────────────────────────────────┐
│               QQ 风格 Web UI                      │
│  纯白背景 · #12B7F5 蓝色主色调 · #9EEA6A 绿气泡   │
│  280px 侧边栏 · 底部导航 · 毛玻璃效果 · 动画      │
├─────────────────────────────────────────────────┤
│               IPC Bridge (C ↔ JS)                │
│  WebSocket 二进制帧, 10 种消息类型 (0x01-0x50)    │
├─────────────────────────────────────────────────┤
│             HTTP/WS 服务器 (C99)                  │
│  事件驱动 (epoll/WSAPoll) + 工作线程池 + 路由     │
│        ├── 用户管理 (注册/登录/搜索/好友)          │
│        ├── 消息服务 (发送/列表/WebSocket 推送)     │
│        ├── 社区功能 (模板上传/下载/应用)           │
│        └── 文件服务 (上传/下载/头像/MIME 检测)     │
├─────────────────────────────────────────────────┤
│        安全模块 (Rust FFI + C 实现)               │
│  Argon2id · JWT HS256 · CSRF 令牌 · 输入验证     │
│  XSS 防护 · 路径遍历防护 · HSTS/CSP 安全头        │
└─────────────────────────────────────────────────┘
```

### 核心组件

| 组件 | 语言 | 说明 |
|------|------|------|
| [`server/`](server/) | C99 | HTTP 服务器、路由处理、数据库、WebSocket |
| [`client/`](client/) | C + HTML/JS | 桌面客户端外壳 + Web UI 前端 |
| [`server/security/`](server/security/) | Rust | 密码哈希 (Argon2id)、JWT 认证 |
| [`client/ui/`](client/ui/) | HTML/CSS/JS | 现代化 Web 用户界面 |

---

## 功能特性

### 测试工具

| 工具 | 说明 |
|------|------|
| [`debug_cli`](server/tools/debug_cli.c) | CLI 调试接口 — 测试 API/WS/数据库、解码 JWT、列出 IPC 类型 |
| [`stress_test`](server/tools/stress_test.c) | 压力测试框架 — 多线程并发、QPS/延迟统计 |
| [`tests/security_pen_test.sh`](tests/security_pen_test.sh) | 安全渗透测试 (SQL/XSS/路径遍历/JWT/CSRF/SSRF/HTTPS降级) |
| [`tests/api_verification_test.sh`](tests/api_verification_test.sh) | API 功能验证 (用户/消息/文件/好友/模板 CRUD) |
| [`tests/loopback_test.sh`](tests/loopback_test.sh) | 端到端回环测试 (14 步全链路) |

### 安全特性

| 类别 | 措施 | 实现 |
|------|------|------|
| **传输加密** | TLS 1.3 (HTTPS-only，强制加密) | [`tls_server.c`](server/src/tls_server.c) — OpenSSL |
| **密码存储** | Argon2id 密码哈希 (抗 GPU/ASIC 攻击) | [`server/security/src/password.rs`](server/security/src/password.rs) — Rust FFI |
| **身份认证** | JWT 令牌签发与验证 (HS256) | [`server/security/src/auth.rs`](server/security/src/auth.rs) — Rust FFI |
| **密钥管理** | 安全密钥存储与初始化 | [`server/security/src/key_mgmt.rs`](server/security/src/key_mgmt.rs) |
| **响应头** | HSTS / CSP / X-Content-Type-Options / X-Frame-Options / Referrer-Policy | [`http_server.c`](server/src/http_server.c):663-672 |
| **JSON 转义** | 输出编码防止 XSS 注入 | [`database.c`](server/src/database.c) + [`user_handler.c`](server/src/user_handler.c) |
| **输入验证** | 用户名/昵称长度与字符限制 | [`user_handler.c`](server/src/user_handler.c) |
| **CSRF 防护** | Origin/Referer 头校验 + CSRF Token 验证 | [`message_handler.c`](server/src/message_handler.c) + [`community_handler.c`](server/src/community_handler.c) |
| **路径遍历防护** | 路径规范化 + 黑名单检测 | [`file_handler.c`](server/src/file_handler.c) |
| **文件类型校验** | 扩展名白名单 + 魔数校验 (头像) | [`file_handler.c`](server/src/file_handler.c) |
| **WebSocket 安全** | Origin 校验 + WSS-only | [`websocket.c`](server/src/websocket.c) |
| **自动重连** | 客户端断线自动恢复，指数退避 | [`network.c`](client/src/network.c) |
| **渗透测试** | 自动化安全测试 (13 类别) | [`tests/security_pen_test.sh`](tests/security_pen_test.sh) |

---

## 快速开始

### 环境要求

**编译工具链**

| 平台 | 编译器 | 依赖 |
|------|--------|------|
| **Linux** | GCC ≥ 4.8 | `libpthread`, `libm` |
| **Windows** | MinGW-w64 GCC | WinSock2 (`-lws2_32`), OpenSSL DLL (`D:\mys32\mingw64\bin`) |

**可选依赖**
- **OpenSSL** (≥ 1.1.0): **必需依赖** (TLS/HTTPS 强制加密传输)
- **Rust** (≥ 1.70): 编译安全模块 (密码哈希 + JWT，可选)
- **NSIS** (≥ 3.0): 制作 Windows 安装包
- **bash**: 运行 shell 测试脚本

### 编译构建

**1. 服务端 (使用 Makefile，推荐)**

```bash
cd server

# 编译全部 (TLS 强制启用，需安装 OpenSSL 开发库)
make build

# 生成自签名测试证书（开发/测试用）
openssl req -x509 -newkey rsa:2048 \
  -keyout certs/server.key -out certs/server.crt \
  -days 365 -nodes \
  -subj "/CN=127.0.0.1" \
  -addext "subjectAltName=IP:127.0.0.1"

# 编译测试工具
make debug-cli
make stress-test

# 清理编译产物
make clean
```

**手动编译 (不使用 Makefile)**

```bash
cd server

# Linux (需要 OpenSSL 开发库)
gcc -std=c99 -Wall -Wextra -Iinclude -DHAS_TLS \
    src/*.c -o build/chrono-server -lpthread -lm -lssl -lcrypto

# Windows (MinGW，需要 MinGW 版 OpenSSL)
gcc -std=c99 -Wall -Wextra -Iinclude -DHAS_TLS \
    src/*.c -o build/chrono-server.exe -lws2_32 -lssl -lcrypto
```

> **TLS 说明**: v0.2.0 起 TLS/HTTPS 为强制要求，OpenSSL 是必需依赖。`tls_stub.c` 桩实现已移除，所有通信必须使用 TLS 加密。

**2. 测试工具**

```bash
cd server

# CLI 调试工具 (启用 TLS)
gcc -std=c99 -Wall -Iinclude tools/debug_cli.c -Iinclude \
    -DHAS_TLS -o build/debug_cli -lws2_32 -lssl -lcrypto

# CLI 调试工具 (无 TLS)
gcc -std=c99 -Wall -Iinclude tools/debug_cli.c -Iinclude \
    -o build/debug_cli -lws2_32

# 压力测试工具
gcc -std=c99 -Wall -Iinclude tools/stress_test.c -o build/stress_test -lws2_32 -lm
```

**3. 安全模块 (可选，需安装 Rust)**

```bash
cd server/security
cargo build --release
# 生成的 libchrono_security.a/.lib 自动被 Makefile 检测并链接
```

### 运行服务器

```bash
cd server

# 启动服务器（强制 TLS，默认端口 4443）
./build/chrono-server --port 4443 --db ./data/db/chrono.db \
    --tls-cert ./certs/server.crt --tls-key ./certs/server.key
```

**命令行选项**

| 选项 | 默认值 | 说明 |
|------|--------|------|
| `--port` | 4443 | 监听端口 |
| `--db` | `./data/db/chrono.db` | 数据库路径 |
| `--storage` | `./data/storage` | 文件存储路径 |
| `--log-level` | 1 | 日志级别 (0-3) |
| `--tls-cert` | - | TLS 证书路径 (PEM) |
| `--tls-key` | - | TLS 私钥路径 (PEM) |
| `--help` | - | 显示帮助信息 |

> **注意**: 从 v0.2.0 起，TLS/HTTPS 为强制要求。服务器必须使用 OpenSSL 加密，不再支持 HTTP 纯文本。
> 如果未指定 `--tls-cert` 和 `--tls-key`，服务器将自动生成自签名证书。
>
> **Windows 注意**: 运行前需将 OpenSSL DLL 目录加入 PATH:
> ```cmd
> set PATH=D:\mys32\mingw64\bin;%PATH%
> build\chrono-server.exe --port 4443 --tls-cert certs\server.crt --tls-key certs\server.key
> ```

启动后访问:
- **HTTPS only**: `https://localhost:4443` (需信任自签名证书或使用受信任 CA)
- HTTP 纯文本已被移除，所有通信强制加密。

### API 快速测试

```bash
# 注册
curl -k -X POST https://localhost:4443/api/user/register \
  -H "Content-Type: application/json" \
  -d '{"username":"test","password":"test123","nickname":"测试用户"}'

# 登录
curl -k -X POST https://localhost:4443/api/user/login \
  -H "Content-Type: application/json" \
  -d '{"username":"test","password":"test123"}'

# 获取用户信息 (使用登录返回的 token)
curl -k https://localhost:4443/api/user/profile?user_id=1 \
  -H "Authorization: Bearer <token>"
```

---

## 项目结构

```
Chrono-shift/
├── server/                       # 服务端
│   ├── src/                      # C 源文件 (17 个模块)
│   │   ├── main.c                # 入口 + 路由注册
│   │   ├── http_server.c         # HTTP 服务器 (epoll/WSAPoll)
│   │   ├── websocket.c           # WebSocket 握手/帧处理
│   │   ├── database.c            # 文件数据库 (JSON 存储)
│   │   ├── user_handler.c        # 用户管理路由 (注册/登录/搜索/好友)
│   │   ├── message_handler.c     # 消息服务 (HTTP + WebSocket)
│   │   ├── community_handler.c   # 社区模板 (上传/下载/应用/列表)
│   │   ├── file_handler.c        # 文件服务 (上传/下载/头像/静态/MIME)
│   │   ├── json_parser.c         # JSON 解析器
│   │   ├── protocol.c            # 通信协议编码
│   │   ├── utils.c               # 工具函数
│   │   ├── tls_server.c          # TLS 实现 (OpenSSL，强制启用)
│   │   ├── tls_stub.c            # TLS 桩 (无 OpenSSL 时)
│   │   └── rust_stubs.c          # Rust FFI 桩函数 (测试用)
│   ├── include/                  # 头文件 (14 个)
│   ├── tools/                    # 测试工具
│   │   ├── debug_cli.c           # CLI 调试 (ws/msg/friend/db 命令)
│   │   └── stress_test.c         # 压力测试框架
│   └── security/                 # Rust 安全模块
│       └── src/                  # Argon2id + JWT + 密钥管理
├── client/                       # 客户端
│   ├── src/                      # C 客户端外壳
│   ├── ui/                       # Web UI (QQ 风格)
│   │   ├── index.html            # 主页面 (QQ 风格布局)
│   │   ├── css/                  # 样式表 (7 个文件)
│   │   │   ├── variables.css     # CSS 变量 (#12B7F5/#9EEA6A)
│   │   │   ├── login.css         # 登录页 (白卡渐变)
│   │   │   ├── main.css          # 主布局 (280px 侧边栏)
│   │   │   ├── chat.css          # 聊天气泡 (绿/白不对称)
│   │   │   ├── community.css     # 社区卡片
│   │   │   ├── global.css        # 全局样式
│   │   │   └── themes/default.css# 默认主题
│   │   ├── js/                   # JavaScript (10 个)
│   │   └── assets/               # 静态资源
│   └── security/                 # Rust 客户端安全模块
├── tests/                        # 测试脚本 (3 个)
│   ├── security_pen_test.sh      # 安全渗透 (10 类, 30+ 测试)
│   ├── api_verification_test.sh  # API 验证 (2.6-2.8 新增)
│   └── loopback_test.sh          # 端到端 (14 步全链路)
├── installer/                    # 安装包脚本
│   ├── client_installer.nsi      # 客户端 NSIS 安装器
│   └── server_installer.nsi      # 服务端 NSIS 安装器
├── docs/                         # 文档
│   ├── API.md                    # API 接口文档
│   ├── PROTOCOL.md               # 通信协议文档
│   ├── BUILD.md                  # 构建指南
│   └── HTTPS_MIGRATION.md        # HTTPS 迁移指南
├── plans/                        # 设计规划
│   ├── ARCHITECTURE.md           # 架构设计文档
│   ├── phase_4_restructure_plan.md
│   ├── phase_5_comprehensive_plan.md
│   ├── phase_6_comprehensive_overhaul_plan.md
│   └── mozhu_testing_ui_plan.md
├── reports/                      # 测试报告
│   └── SUMMARY.md                # 功能总结报告
├── cleanup.bat                   # Windows 清理脚本
├── cleanup.sh                    # Linux 清理脚本
├── LICENSE                       # GPLv3 许可证
└── README.md                     # 本文件
```

---

## API 文档

完整 API 文档请参见 [`docs/API.md`](docs/API.md)。

### 路由总览

| 方法 | 路径 | 说明 | 处理器 |
|------|------|------|--------|
| POST | `/api/user/register` | 用户注册 | [`user_handler.c`](server/src/user_handler.c) |
| POST | `/api/user/login` | 用户登录 | ↑ |
| GET | `/api/user/profile?user_id=X` | 获取用户信息 | ↑ |
| PUT | `/api/user/update` | 更新用户资料 | ↑ |
| GET | `/api/user/search?keyword=X` | 搜索用户 | ↑ |
| GET | `/api/user/friends` | 好友列表 | ↑ |
| POST | `/api/user/friends/add` | 添加好友 | ↑ |
| POST | `/api/message/send` | 发送消息 | [`message_handler.c`](server/src/message_handler.c) |
| GET | `/api/message/list?user_id=X` | 消息历史 | ↑ |
| GET | `/api/templates` | 模板列表 | [`community_handler.c`](server/src/community_handler.c) |
| POST | `/api/templates/upload` | 上传模板 | ↑ |
| GET | `/api/templates/download` | 下载模板 | ↑ |
| POST | `/api/templates/apply` | 应用模板 | ↑ |
| POST | `/api/file/upload` | 上传文件 | [`file_handler.c`](server/src/file_handler.c) |
| GET | `/api/file/*` | 静态文件/下载 | ↑ |
| POST | `/api/avatar/upload` | 上传头像 | ↑ |

---

## IPC 协议

IPC (进程间通信) 基于 WebSocket + 二进制消息帧，支持 **10 种消息类型**：

| 类型码 | 名称 | 说明 | 方向 |
|--------|------|------|------|
| `0x01` | `LOGIN` | 用户登录 | JS → C |
| `0x02` | `LOGOUT` | 用户登出 | JS → C |
| `0x03` | `MESSAGE` | 消息发送 | 双向 |
| `0x04` | `MESSAGE_ACK` | 消息确认 | C → JS |
| `0x05` | `SYSTEM_NOTIFY` | 系统通知 | C → JS |
| `0x06` | `HEARTBEAT` | 心跳检测 | 双向 |
| `0x07` | `FILE_TRANSFER` | 文件传输 | 双向 |
| `0x08` | `FRIEND_REQUEST` | 好友请求 | 双向 |
| `0x09` | `FRIEND_RESPONSE` | 好友响应 | 双向 |
| `0x50` | `OPEN_URL` | 打开外部链接 | JS → C |

**协议文件**: [`ipc_bridge.h`](client/include/ipc_bridge.h) (C 定义) + [`ipc.js`](client/ui/js/ipc.js) (JS 实现)

详见 [`docs/PROTOCOL.md`](docs/PROTOCOL.md)。

---

## CLI 调试工具

[`debug_cli`](server/tools/debug_cli.c) 是一个强大的 CLI 调试接口，支持以下命令：

| 命令 | 子命令 | 说明 |
|------|--------|------|
| `help` | — | 显示所有可用命令 |
| `ipc types` | — | 列出 IPC 消息类型 |
| `ipc send` | `<hex> <json>` | 发送 IPC 消息 |
| `user` | `register/login/profile/search` | 用户管理 |
| `ws` | `connect/send/recv/close/status` | WebSocket 调试 (E1) |
| `msg` | `list/get/send` | 数据库消息操作 (E2) |
| `friend` | `list/add` | 好友管理 (E2) |
| `db` | `list` | 数据库内容浏览 (E2) |
| `exit/quit` | — | 退出 |

## 测试

### 编译测试

```bash
# 服务端编译测试 (使用 Makefile)
cd server

# 编译全部 (TLS 强制启用)
mingw32-make build

# Windows: 需先将 OpenSSL DLL 加入 PATH
set PATH=D:\mys32\mingw64\bin;%PATH%

# 启动服务器（端口 4443）
out\chrono-server.exe --port 4443

# 使用 openssl 验证 TLS 连接
openssl s_client -connect 127.0.0.1:4443
```

### Shell 测试脚本 (需 bash)

```bash
# API 功能验证 (用户/消息/文件/好友/模板 CRUD)
bash tests/api_verification_test.sh

# 安全渗透测试 (SQL/XSS/路径遍历/JWT/CSRF/SSRF/HTTPS降级)
bash tests/security_pen_test.sh

# 端到端回环测试 (14 步全链路)
bash tests/loopback_test.sh
```

### 测试覆盖

| 测试脚本 | 覆盖范围 | 测试数 |
|----------|---------|--------|
| [`security_pen_test.sh`](tests/security_pen_test.sh) | SQL注入/XSS/路径遍历/JWT伪造/权限绕过/大负载/输入验证/CSRF(5)/SSRF(6)/HTTPS降级(2) | 30+ |
| [`api_verification_test.sh`](tests/api_verification_test.sh) | 用户系统/消息/文件系统/好友系统/模板CRUD/边界情况 | 20+ |
| [`loopback_test.sh`](tests/loopback_test.sh) | 全链路 14 步 (注册→登录→资料→消息→文件→模板→好友→CLI) | 14 |

### 压力测试

```bash
cd server
out/stress_test --host 127.0.0.1 --port 4443 --threads 4 --qps 100 --duration 30
```

---

## 安装包制作

本项目提供 NSIS (Nullsoft Scriptable Install System) 安装脚本，位于 [`installer/`](installer/) 目录。

### 准备工作

1. 安装 [NSIS](https://nsis.sourceforge.io/Download) (≥ 3.0)
2. 编译服务端和所有测试工具
3. 确保 `makensis` 在 PATH 中

### 制作客户端安装包

```bash
makensis installer/client_installer.nsi
# 输出: installer/Chrono-shift-Client-Setup.exe
```

### 制作服务端安装包

```bash
makensis installer/server_installer.nsi
# 输出: installer/Chrono-shift-Server-Setup.exe
```

---

## 开源协议

本项目采用 **GNU General Public License v3.0 (GPLv3)** 开源协议。

**Copyright (C) 2026 haiyanfurry**

```
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
```

### 商业许可

GPLv3 允许自由使用、修改和分发，但如果您需要在 **闭源/商业项目** 中使用本代码，请联系版权方获取商业许可证。

详情请参见 [`LICENSE`](LICENSE) 文件中的 "ADDITIONAL PERMISSION" 章节。

---

## 联系方式

- **项目维护**: haiyanfurry
- **邮箱**: [2752842448@qq.com](mailto:2752842448@qq.com)
- **商业许可咨询**: [2752842448@qq.com](mailto:2752842448@qq.com)

---

> **Chrono-shift (墨竹)** — 跨越时间的连接
