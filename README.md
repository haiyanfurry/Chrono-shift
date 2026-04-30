# Chrono-shift (墨竹)

> 一个轻量级即时通讯与社区平台 — 跨平台、模块化、可扩展

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

**Chrono-shift (墨竹)** 是一款使用纯 C99 (服务端) + Web 技术 (客户端) 构建的即时通讯平台，支持用户注册登录、消息收发、社区互动、文件传输等功能。

后端采用 epoll/poll 事件驱动架构，前端为现代化 Web UI，安全模块使用 Rust 编写 (Argon2id 密码哈希 + JWT 认证)。

### 设计目标

- **轻量**: 纯 C 编写，低内存占用 (≈ 10MB)
- **跨平台**: Windows (WSAPoll) / Linux (epoll)
- **安全**: Argon2id 密码哈希 + JWT 令牌认证
- **可扩展**: 模块化路由设计，预留 IPC 消息接口

---

## 技术架构

```
┌─────────────────────────────────────────────────┐
│                   客户端 (Web UI)                  │
│  HTML5 + CSS3 + JavaScript (原生, 无框架依赖)     │
├─────────────────────────────────────────────────┤
│                   IPC Bridge                     │
│  (WebSocket ↔ 本地消息队列, 9 种消息类型)         │
├─────────────────────────────────────────────────┤
│                 HTTP 服务器 (C99)                 │
│  事件驱动 (epoll/LSAPoll) + 工作线程池 + 路由     │
│        ├── 用户管理 (注册/登录/搜索)              │
│        ├── 消息服务 (发送/列表)                   │
│        ├── 社区功能 (圈子/模板)                   │
│        └── 文件服务 (上传/下载/头像)              │
├─────────────────────────────────────────────────┤
│              安全模块 (Rust FFI)                  │
│     Argon2id 密码哈希  |  JWT 令牌签发/验证       │
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

### 已完成功能

- **用户系统**: 注册、登录、资料查看/编辑、搜索用户
- **消息系统**: 文本消息发送与历史列表 (Phase 4 开发中)
- **社区功能**: 圈子、模板管理 (Phase 5 开发中)
- **文件服务**: 文件上传、下载、头像管理 (Phase 5 开发中)
- **WebSocket**: 实时通信支持 (基础框架完成)
- **Web UI**: 现代简约风格界面，支持暗色主题
  - 登录/注册页 (渐变背景 + 毛玻璃效果)
  - 聊天界面 (非对称气泡 + 输入框)
  - 侧边栏 (用户头像、导航、外部链接)
  - 联系人列表、社区面板

### 测试工具

| 工具 | 说明 |
|------|------|
| [`debug_cli`](server/tools/debug_cli.c) | CLI 调试接口 — 测试 API、解码 JWT、列出 IPC 类型 |
| [`stress_test`](server/tools/stress_test.c) | 压力测试框架 — 多线程并发、QPS/延迟统计 |
| [`tests/security_pen_test.sh`](tests/security_pen_test.sh) | 安全渗透测试脚本 |
| [`tests/api_verification_test.sh`](tests/api_verification_test.sh) | API 功能验证脚本 |
| [`tests/loopback_test.sh`](tests/loopback_test.sh) | 端到端回环测试脚本 |

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
| **自动重连** | 客户端断线自动恢复，指数退避 | [`network.c`](client/src/network.c) |
| **渗透测试** | 自动化安全测试脚本 | [`tests/security_pen_test.sh`](tests/security_pen_test.sh) |

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
├── server/                    # 服务端
│   ├── src/                   # C 源文件
│   │   ├── main.c             # 入口 + 路由注册
│   │   ├── http_server.c      # HTTP 服务器 (epoll/WSAPoll)
│   │   ├── websocket.c        # WebSocket 握手/帧处理
│   │   ├── database.c         # 文件数据库 (JSON)
│   │   ├── user_handler.c     # 用户管理路由
│   │   ├── message_handler.c  # 消息服务路由
│   │   ├── community_handler.c# 社区功能路由
│   │   ├── file_handler.c     # 文件服务路由
│   │   ├── json_parser.c      # JSON 解析器
│   │   ├── protocol.c         # 通信协议编码
│   │   ├── utils.c            # 工具函数
│   │   ├── tls_server.c       # TLS 实现 (OpenSSL，强制启用)
│   │   └── rust_stubs.c       # Rust FFI 桩函数 (测试用)
│   ├── include/               # 头文件
│   ├── tools/                 # 测试工具
│   │   ├── debug_cli.c        # CLI 调试接口
│   │   └── stress_test.c      # 压力测试框架
│   └── security/              # Rust 安全模块
│       └── src/               # Argon2id + JWT
├── client/                    # 客户端
│   ├── src/                   # C 客户端外壳
│   ├── ui/                    # Web UI
│   │   ├── index.html         # 主页面
│   │   ├── css/               # 样式表
│   │   ├── js/                # JavaScript 逻辑
│   │   └── assets/            # 静态资源
│   └── security/              # Rust 客户端安全模块
├── tests/                     # 测试脚本
│   ├── security_pen_test.sh   # 安全渗透测试
│   ├── api_verification_test.sh  # API 验证
│   └── loopback_test.sh       # 端到端测试
├── installer/                 # 安装包脚本
│   ├── client_installer.nsi   # 客户端 NSIS 安装器
│   └── server_installer.nsi   # 服务端 NSIS 安装器
├── docs/                      # 文档
│   ├── API.md                 # API 接口文档
│   ├── PROTOCOL.md            # 通信协议文档
│   └── BUILD.md               # 构建指南
├── plans/                     # 设计规划
│   ├── ARCHITECTURE.md        # 架构设计文档
│   ├── phase_4_restructure_plan.md  # Phase 4 重构计划
│   └── mozhu_testing_ui_plan.md     # 测试与 UI 改造计划
├── reports/                   # 测试报告
│   └── SUMMARY.md             # 功能总结报告
├── cleanup.bat                # Windows 清理脚本
├── cleanup.sh                 # Linux 清理脚本
├── LICENSE                    # GPLv3 许可证
└── README.md                  # 本文件
```

---

## API 文档

完整 API 文档请参见 [`docs/API.md`](docs/API.md)。

### 路由总览

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
| POST | `/api/file/upload` | 上传文件 |
| GET | `/api/file/*` | 下载文件 |
| POST | `/api/avatar/upload` | 上传头像 |

---

## IPC 协议

IPC (进程间通信) 基于 WebSocket + 二进制消息帧，支持 9 种消息类型：

| 类型码 | 名称 | 说明 |
|--------|------|------|
| `0x01` | `LOGIN` | 用户登录 |
| `0x02` | `LOGOUT` | 用户登出 |
| `0x03` | `MESSAGE` | 消息发送 |
| `0x04` | `MESSAGE_ACK` | 消息确认 |
| `0x05` | `SYSTEM_NOTIFY` | 系统通知 |
| `0x06` | `HEARTBEAT` | 心跳检测 |
| `0x07` | `FILE_TRANSFER` | 文件传输 |
| `0x08` | `FRIEND_REQUEST` | 好友请求 |
| `0x09` | `FRIEND_RESPONSE` | 好友响应 |
| `0x50` | `OPEN_URL` | 打开外部链接 |

详见 [`docs/PROTOCOL.md`](docs/PROTOCOL.md)。

---

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
build\chrono-server.exe --port 4443

# 使用 openssl 验证 TLS 连接
openssl s_client -connect 127.0.0.1:4443
```

### Shell 测试脚本 (需 bash)

```bash
# API 功能验证
bash tests/api_verification_test.sh

# 安全渗透测试
bash tests/security_pen_test.sh

# 端到端回环测试
bash tests/loopback_test.sh
```

### 压力测试

```bash
cd server
./build/stress_test --host 127.0.0.1 --port 4443 --threads 4 --qps 100 --duration 30
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
