# Chrono-shift (墨竹) — QQ 风格社交平台客户端

> 一个轻量级即时通讯与社区平台 — QQ 风格 UI、跨平台、模块化、可扩展

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)
![Language: C/C99 + Rust + JavaScript](https://img.shields.io/badge/language-C%20%7C%20C%2B%2B%20%7C%20Rust%20%7C%20JavaScript-blue)
![Platform: Windows + Linux](https://img.shields.io/badge/platform-Windows%20%7C%20Linux-lightgrey)

---

## 目录

- [项目概述](#项目概述)
- [技术架构](#技术架构)
- [功能特性](#功能特性)
- [快速开始](#快速开始)
  - [环境要求](#环境要求)
  - [编译构建](#编译构建)
- [项目结构](#项目结构)
- [API 文档](#api-文档)
- [IPC 协议](#ipc-协议)
- [CLI 调试工具](#cli-调试工具)
- [测试](#测试)
- [安装包制作](#安装包制作)
- [开源协议](#开源协议)
- [联系方式](#联系方式)

---

## 项目概述

**Chrono-shift (墨竹)** 是一款使用 C/C++ (客户端外壳) + Web 技术 (UI) + Rust (安全模块) 构建的即时通讯桌面客户端，采用 **QQ 风格用户界面**，支持用户注册登录、消息收发、社区互动、文件传输、模板自定义等功能。

**v0.3.0 重构**: 服务端 (`server/`) 已移除，项目聚焦于**桌面客户端**和 **CLI 工具**。
- 桌面客户端: C++ (WebView2/WebKitGTK) + QQ 风格 Web UI
- CLI 工具: 纯 C 调试接口 (`debug_cli`) + 压力测试 (`stress_test`)
- 安全模块: Rust FFI (AES-256-GCM 加密/会话管理/安全存储)

### 设计目标

- **轻量**: 纯 C/C++ 编写，低内存占用
- **跨平台**: Windows (WebView2) / Linux (WebKitGTK)
- **安全**: AES-256-GCM E2E 加密 + 会话管理 + 安全本地存储
- **可扩展**: 模块化设计（网络/安全/存储/UI），IPC 消息接口
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
│            客户端网络层 (C/C++)                    │
│  TCP/TLS/HTTP/WebSocket 客户端实现                │
│  自动重连 · 指数退避 · 连接池管理                  │
├─────────────────────────────────────────────────┤
│        安全模块 (Rust FFI + C 实现)               │
│  AES-256-GCM · 会话管理 · 安全存储                 │
│  令牌管理 · E2E 加密                              │
├─────────────────────────────────────────────────┤
│              CLI 工具 (纯 C)                      │
│  debug_cli — 调试/测试/诊断                       │
│  stress_test — 压力测试/性能评估                  │
└─────────────────────────────────────────────────┘
```

### 核心组件

| 组件 | 语言 | 说明 |
|------|------|------|
| [`client/src/network/`](client/src/network/) | C/C++ | TCP/HTTP/WebSocket/TLS 客户端实现 |
| [`client/src/app/`](client/src/app/) | C++ | 桌面客户端外壳 (WebView2/WebKitGTK 集成) |
| [`client/src/security/`](client/src/security/) | C++ | 加密引擎、令牌管理器 |
| [`client/src/storage/`](client/src/storage/) | C++ | 本地存储、会话管理 |
| [`client/src/util/`](client/src/util/) | C++ | 工具函数、日志 |
| [`client/security/`](client/security/) | Rust | AES-256-GCM、安全存储、会话管理 |
| [`client/ui/`](client/ui/) | HTML/CSS/JS | 现代化 Web 用户界面 |
| [`client/tools/`](client/tools/) | C | CLI 调试工具 + 压力测试工具 |

---

## 功能特性

### CLI 调试工具

| 命令 | 子命令 | 说明 |
|------|--------|------|
| `session` | `show/login/logout` | 会话管理 |
| `config` | `show/set` | 客户端配置 |
| `storage` | `list/get` | 安全本地存储 |
| `crypto` | `test` | AES-256-GCM 加密测试 |
| `network` | `test <host> <port>` | 网络连通性诊断 (DNS/TCP/TLS/HTTP) |
| `ipc` | `types/send/capture` | IPC 消息类型/发送/捕获 |
| `ws` | `connect/send/recv/close/status/monitor` | WebSocket 调试 |
| `health` | — | 服务器健康检查 |
| `ping` | `[count]` | 服务器延迟测试 |
| `watch` | `[interval] [rounds]` | 实时监控 |

### 安全特性

| 类别 | 措施 | 实现 |
|------|------|------|
| **传输加密** | TLS 1.3 (客户端连接) | [`tls_client.c`](client/src/network/tls_client.c) — OpenSSL |
| **E2E 加密** | AES-256-GCM 端到端加密 | [`client/security/src/crypto.rs`](client/security/src/crypto.rs) — Rust FFI |
| **安全存储** | AES-256-GCM 加密本地存储 | [`client/security/src/secure_storage.rs`](client/security/src/secure_storage.rs) — Rust FFI |
| **会话管理** | 令牌保存/验证/清除 | [`client/security/src/session.rs`](client/security/src/session.rs) — Rust FFI |
| **自动重连** | 断线自动恢复，指数退避 | [`network.c`](client/src/network.c) |

---

## 快速开始

### 环境要求

**编译工具链**

| 平台 | 编译器 | 依赖 |
|------|--------|------|
| **Linux** | GCC ≥ 4.8, G++ ≥ 8 | `libssl-dev`, `libwebkit2gtk-4.1-dev` |
| **Windows** | MinGW-w64 GCC | WinSock2 (`-lws2_32`), OpenSSL DLL |

**可选依赖**
- **OpenSSL** (≥ 1.1.0): **必需依赖** (TLS 加密传输)
- **Rust** (≥ 1.70): 编译安全模块 (AES-256-GCM)
- **NSIS** (≥ 3.0): 制作 Windows 安装包

### 编译构建

**1. CLI 工具**

```bash
# Linux
cd client/tools
gcc -std=c99 -Wall -I../include debug_cli.c -o debug_cli -lssl -lcrypto -lm
gcc -std=c99 -Wall -I../include stress_test.c -o stress_test -lm

# Windows (MinGW)
cd client/tools
gcc -std=c99 -Wall -I../include debug_cli.c -o debug_cli.exe -lws2_32 -lssl -lcrypto
gcc -std=c99 -Wall -I../include stress_test.c -o stress_test.exe -lws2_32 -lm
```

**2. 安全模块 (可选，需安装 Rust)**

```bash
cd client/security
cargo build --release
```

**3. 桌面客户端**

```bash
cd client
cmake -B build -G "MinGW Makefiles"  # Windows
cmake -B build                        # Linux
cmake --build build
```

详细构建说明请参见 [`docs/BUILD.md`](docs/BUILD.md)。

---

## 项目结构

```
Chrono-shift/
├── client/                       # 桌面客户端 + CLI 工具
│   ├── include/                  # 头文件
│   │   ├── tls_client.h          # TLS 客户端 API
│   │   ├── platform_compat.h     # 跨平台兼容
│   │   ├── json_parser.h         # JSON 解析器
│   │   ├── protocol.h            # 通信协议
│   │   ├── client.h              # 客户端主接口
│   │   └── ...
│   ├── src/                      # C/C++ 源码
│   │   ├── network/              # 网络通信 (TCP/TLS/HTTP/WS)
│   │   │   ├── tls_client.c      # TLS 客户端 (OpenSSL)
│   │   │   ├── TlsWrapper.cpp    # C++ TLS 包装
│   │   │   ├── TcpConnection.cpp # TCP 连接
│   │   │   ├── HttpConnection.cpp# HTTP 请求
│   │   │   ├── WebSocketClient.cpp # WebSocket 客户端
│   │   │   └── NetworkClient.cpp # 网络客户端管理器
│   │   ├── security/             # 安全引擎
│   │   │   ├── CryptoEngine.cpp  # 加密引擎
│   │   │   └── TokenManager.cpp  # 令牌管理
│   │   ├── storage/              # 本地存储
│   │   │   ├── LocalStorage.cpp  # 本地持久化
│   │   │   └── SessionManager.cpp# 会话管理
│   │   ├── app/                  # 应用外壳
│   │   │   ├── Main.cpp          # 入口
│   │   │   ├── WebViewManager.cpp# WebView2/WebKitGTK
│   │   │   ├── IpcBridge.cpp     # IPC 桥接
│   │   │   └── ClientHttpServer.cpp # 客户端 HTTP 服务
│   │   └── util/                 # 工具
│   │       ├── Logger.cpp        # 日志
│   │       └── Utils.cpp         # 通用工具
│   ├── tools/                    # CLI 工具
│   │   ├── debug_cli.c           # CLI 调试接口
│   │   ├── stress_test.c         # 压力测试框架
│   │   └── Makefile              # CLI 工具构建
│   ├── ui/                       # 前端 UI
│   │   ├── index.html            # 主页面
│   │   ├── css/                  # 样式表 (7 个文件)
│   │   ├── js/                   # JavaScript (10 个)
│   │   └── assets/               # 静态资源
│   ├── security/                 # Rust 安全模块
│   │   └── src/
│   │       ├── lib.rs            # FFI 接口
│   │       ├── crypto.rs         # AES-256-GCM 加密
│   │       ├── session.rs        # 会话管理
│   │       └── secure_storage.rs # 安全存储
│   └── CMakeLists.txt
├── tests/                        # 测试脚本
│   ├── security_pen_test.sh      # 安全渗透
│   ├── api_verification_test.sh  # API 验证
│   └── loopback_test.sh          # 端到端测试
├── installer/                    # 安装包脚本
│   └── client_installer.nsi      # 客户端安装器
├── docs/                         # 文档
│   ├── API.md                    # API 接口文档
│   ├── PROTOCOL.md               # 通信协议文档
│   ├── BUILD.md                  # 构建指南
│   └── HTTPS_MIGRATION.md        # HTTPS 迁移指南
├── plans/                        # 设计规划
├── reports/                      # 测试报告
├── cleanup.bat                   # Windows 清理脚本
├── cleanup.sh                    # Linux 清理脚本
└── README.md                     # 本文件
```

---

## API 文档

完整 API 文档请参见 [`docs/API.md`](docs/API.md)。

### API 端点

| 方法 | 路径 | 说明 |
|------|------|------|
| POST | `/api/user/register` | 用户注册 |
| POST | `/api/user/login` | 用户登录 |
| GET | `/api/user/profile?user_id=X` | 获取用户信息 |
| PUT | `/api/user/update` | 更新用户资料 |
| GET | `/api/user/search?keyword=X` | 搜索用户 |
| GET | `/api/user/friends` | 好友列表 |
| POST | `/api/user/friends/add` | 添加好友 |
| POST | `/api/message/send` | 发送消息 |
| GET | `/api/message/list?user_id=X` | 消息历史 |
| GET | `/api/templates` | 模板列表 |
| POST | `/api/templates/upload` | 上传模板 |
| POST | `/api/templates/download` | 下载模板 |
| POST | `/api/templates/apply` | 应用模板 |
| POST | `/api/file/upload` | 上传文件 |
| GET | `/api/file/*` | 静态文件/下载 |
| POST | `/api/avatar/upload` | 上传头像 |

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

[`debug_cli`](client/tools/debug_cli.c) 是一个强大的 CLI 调试接口，支持以下命令：

| 命令 | 子命令 | 说明 |
|------|--------|------|
| `help` | — | 显示所有可用命令 |
| `session` | `show/login/logout` | 会话管理 |
| `config` | `show/set <key> <value>` | 配置管理 |
| `storage` | `list/get <key>` | 安全存储 |
| `crypto` | `test` | AES-256-GCM 加密测试 |
| `network` | `test <host> <port>` | 网络诊断 |
| `ipc` | `types/send/capture` | IPC 消息 |
| `health` | — | 服务器健康检查 |
| `endpoint` | `<path> [method] [body]` | 测试 API 端点 |
| `token` | `<jwt>` | 解码 JWT |
| `user` | `list/get/create/delete` | 用户管理 |
| `ws` | `connect/send/recv/close/status/monitor` | WebSocket 调试 |
| `msg` | `list/get/send` | 消息操作 |
| `friend` | `list/add` | 好友管理 |
| `db` | `list <type>` | 数据库内容浏览 |
| `connect` | `<host> <port> [tls]` | 连接服务器 |
| `disconnect` | — | 断开连接 |
| `tls-info` | — | TLS 连接信息 |
| `json-parse` / `json-pretty` | `<str>` | JSON 工具 |
| `trace` | `<path>` | 请求追踪 |
| `ping` | `[count]` | 延迟测试 |
| `watch` | `[interval] [rounds]` | 实时监控 |
| `rate-test` | `[n]` | 速率测试 |
| `verbose` | — | 切换详细模式 |
| `exit / quit` | — | 退出 |

### 使用示例

```bash
# 交互模式
./client/tools/debug_cli

# 单命令模式
./client/tools/debug_cli health
./client/tools/debug_cli "config show"
./client/tools/debug_cli "crypto test"
./client/tools/debug_cli "network test 127.0.0.1 4443"
./client/tools/debug_cli "session login 127.0.0.1 <token>"
./client/tools/debug_cli "ws monitor 5 2"
```

---

## 测试

### 编译测试

```bash
# 编译 CLI 工具
cd client/tools
gcc -std=c99 -Wall -I../include debug_cli.c -o debug_cli -lws2_32 -lssl -lcrypto

# 运行 CLI 并测试连接
./debug_cli health
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

---

## 安装包制作

本项目提供 NSIS (Nullsoft Scriptable Install System) 安装脚本，位于 [`installer/`](installer/) 目录。

### 准备工作

1. 安装 [NSIS](https://nsis.sourceforge.io/Download) (≥ 3.0)
2. 编译客户端和所有 CLI 工具
3. 确保 `makensis` 在 PATH 中

### 制作客户端安装包

```bash
makensis installer/client_installer.nsi
# 输出: installer/Chrono-shift-Client-Setup.exe
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
