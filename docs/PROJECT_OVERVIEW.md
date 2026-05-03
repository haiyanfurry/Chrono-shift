# Chrono-shift (墨竹) — 项目全景概览

> **版本**: v2.0.0
> **项目定位**: 跨平台即时通讯桌面客户端 (QQ 风格)
> **目标用户**: 二次元 / furry 社区
> **架构特征**: 纯客户端架构 (无服务端)、多层安全加密、AI 多提供商集成、NASM 私有混淆

---

## 目录

- [1. 项目简介](#1-项目简介)
- [2. 快速开始](#2-快速开始)
- [3. 架构全景](#3-架构全景)
- [4. 模块详解](#4-模块详解)
  - [4.1 网络层](#41-网络层)
  - [4.2 安全层](#42-安全层)
  - [4.3 AI 模块](#43-ai-模块)
  - [4.4 插件系统](#44-插件系统)
  - [4.5 DevTools 开发者工具](#45-devtools-开发者工具)
  - [4.6 前端 UI](#46-前端-ui)
  - [4.7 存储层](#47-存储层)
- [5. 数据流详解](#5-数据流详解)
  - [5.1 消息发送流程](#51-消息发送流程)
  - [5.2 登录流程](#52-登录流程)
  - [5.3 AI 对话流程](#53-ai-对话流程)
- [6. 开发指南](#6-开发指南)
  - [6.1 添加新的 AI 提供商](#61-添加新的-ai-提供商)
  - [6.2 创建新插件](#62-创建新插件)
  - [6.3 添加新 CLI 命令](#63-添加新-cli-命令)
  - [6.4 修改前端 UI](#64-修改前端-ui)
- [7. 测试指南](#7-测试指南)
- [8. 部署与发布](#8-部署与发布)
- [9. 附录](#9-附录)

---

## 1. 项目简介

### 1.1 背景

Chrono-shift (墨竹) 是一个面向二次元 / furry 社区的跨平台即时通讯桌面客户端。项目采用 **QQ 风格 UI** 设计，提供熟悉的社交体验，同时引入多层安全机制以保护用户隐私。

### 1.2 核心特性

| 特性 | 说明 |
|------|------|
| 🖥️ **QQ 风格 UI** | 三栏布局 (侧栏/联系人/对话)，深色/浅色主题 |
| 🔒 **多层加密** | HTTPS/TLS + E2E (AES-256-GCM) + ASM 私有混淆 (ChronoStream v1) |
| 🤖 **AI 多提供商** | 支持 6 种 AI 提供商：OpenAI / DeepSeek / xAI Grok / Ollama / Gemini / 自定义 |
| 🧩 **插件系统** | 基于 JSON manifest 的插件架构，支持运行时加载/卸载 |
| 🛠️ **DevTools CLI** | 30+ 命令行工具，覆盖网络/加密/会话/WebSocket 调试 |
| 🔐 **安全存储** | Rust 实现的安全存储，AES-256 加密本地数据 |
| 🧬 **NASM 私有加密** | 自研 ChronoStream v1 流密码，纯 ASM 实现 |

### 1.3 版本历史

| 版本 | 日期 | 主要变更 |
|------|------|----------|
| v0.1.0 | - | 初始原型 (C89 + 服务端) |
| v0.2.0 | - | TLS/HTTPS 迁移完成 |
| v0.3.0 | - | 服务端移除，纯客户端架构，C++17 迁移 |
| v2.0.0 | 2026-05 | ASM 混淆、AI 多提供商、插件系统、DevTools CLI 完成 |

### 1.4 技术栈

| 层 | 技术 | 用途 |
|----|------|------|
| 应用外壳 | C++17 | 主程序、IPC、HTTP 服务器窗口管理 |
| 安全核心 | Rust 2021 | E2E 加密、ASM FFI 桥接、安全存储、输入校验 |
| 私有加密 | NASM x64 | ChronoStream v1 自研流密码 |
| 前端 UI | JavaScript + HTML + CSS | QQ 风格界面、AI 对话、插件运行环境 |
| 构建 | CMake + Cargo + Makefile | 多语言混合构建 |
| 安装 | NSIS (Windows) | 安装包制作 |
| 传输 | OpenSSL / WebSocket | TCP/TLS/WS 网络通信 |

---

## 2. 快速开始

### 2.1 环境要求

| 组件 | 最低版本 | 说明 |
|------|----------|------|
| GCC (MinGW) | 13.0+ | Windows 推荐 winlibs.com |
| CMake | 3.15+ | 构建系统 |
| Rust | 1.70+ | 安全模块 |
| NASM | 3.01+ | ASM 私有加密 |
| OpenSSL | 1.1+ | TLS 传输加密 |
| WebView2 | 内置 | Windows 10/11 运行时 |
| NSIS | 3.12+ | 安装包制作 (可选) |

### 2.2 构建步骤

```bash
# 1. 编译 Rust 安全模块 (含 ASM)
cd client/security
cargo build --release
cd ../..

# 2. 编译 DevTools CLI
cd client/devtools/cli
make
cd ../../..

# 3. 编译桌面客户端 (C++)
cd client
cmake -B build -G "MinGW Makefiles"
cmake --build build
cd ..

# 4. (可选) 生成自签名证书
openssl req -x509 -newkey rsa:2048 \
  -keyout client/certs/server.key -out client/certs/server.crt \
  -days 365 -nodes -subj "/CN=127.0.0.1" \
  -addext "subjectAltName=IP:127.0.0.1"
```

### 2.3 运行

```bash
# 启动客户端
./client/build/Release/chrono-client.exe

# 或使用 DevTools CLI 测试连接
./client/devtools/cli/chrono-devtools health
./client/devtools/cli/chrono-devtools connect --host 127.0.0.1 --port 4443
```

---

## 3. 架构全景

### 3.1 七层架构

```
┌──────────────────────────────────────────────────────────────┐
│                    Web UI (JavaScript)                        │
│  QQ 风格界面 · AI 对话 · 通讯录 · 社区 · 设置 · 插件运行环境 │
└──────────────────────────┬───────────────────────────────────┘
                           │ IPC (HTTP + JSON)
┌──────────────────────────▼───────────────────────────────────┐
│                  App Layer (C++17)                            │
│  ClientHttpServer · IpcBridge · WebViewManager · Updater      │
│  AppContext · Main                                            │
└──────────────────────────┬───────────────────────────────────┘
                           │
┌──────────────────────────▼───────────────────────────────────┐
│               Network Layer (C/C++ + OpenSSL)                 │
│  TcpConnection · WebSocketClient · TlsWrapper · HttpConnection│
│  NetworkClient · tls_client (C) · Sha1                       │
└──────────────────────────┬───────────────────────────────────┘
                           │
┌──────────────────────────▼───────────────────────────────────┐
│           Security Layer (C++ → Rust → NASM)                  │
│                                                               │
│  ┌─────────────────┐  ┌──────────────────┐  ┌──────────────┐ │
│  │ CryptoEngine     │  │ TokenManager      │  │ SessionMgr   │ │
│  │ (C++ 统一接口)   │  │ (令牌管理)        │  │ (会话管理)   │ │
│  └────────┬────────┘  └──────────────────┘  └──────────────┘ │
│           │ FFI extern "C"                                     │
│  ┌────────▼────────────────────────────────────┐               │
│  │        Rust FFI 调度层 (chrono_client_security)            │
│  │  crypto.rs · asm_bridge.rs · sanitizer.rs                  │
│  │  secure_storage.rs · session.rs                            │
│  └────────┬────────────────────────────────────┘              │
│           │ extern "C"                                         │
│  ┌────────▼────────────────────────────────────┐               │
│  │  NASM 私有加密核心 (ChronoStream v1)                        │
│  │  obfuscate.asm — ksa_init + gen_keystream                  │
│  └─────────────────────────────────────────────┘              │
└──────────────────────────────────────────────────────────────┘
                           │
┌──────────────────────────▼───────────────────────────────────┐
│            AI Module (C++17 → HTTP → Provider API)             │
│  AIProvider · OpenAIProvider · GeminiProvider · CustomProvider │
│  AIConfig · AIChatSession                                     │
│  支持的提供商: OpenAI · DeepSeek · xAI Grok                   │
│               Ollama · Gemini · 自定义 API                    │
└──────────────────────────────────────────────────────────────┘
                           │
┌──────────────────────────▼───────────────────────────────────┐
│         Plugin System (JSON manifest + JS sandbox)            │
│  PluginManager · PluginManifest · 运行时注册/注销             │
│  示例: plugin_catalog.json + example_plugin (manifest + JS)   │
└──────────────────────────────────────────────────────────────┘
                           │
┌──────────────────────────▼───────────────────────────────────┐
│         DevTools (独立 CLI + In-App DevTools)                  │
│  ┌───────────────────┐  ┌────────────────────────────────┐   │
│  │ CLI (C99)         │  │ In-App DevTools (C++ + JS)     │   │
│  │ 30+ 个命令        │  │ DevToolsEngine · DevToolsHttpApi│   │
│  │ 交互/单命令模式   │  │ DevToolsIpcHandler · devtools.js│   │
│  └───────────────────┘  └────────────────────────────────┘   │
└──────────────────────────────────────────────────────────────┘
```

### 3.2 目录结构

```
chrono-shift/
├── client/                          # 桌面客户端
│   ├── CMakeLists.txt               # 顶层 CMake 构建
│   ├── include/                     # 公共 C 头文件
│   │   ├── client_http_server.h
│   │   ├── ipc_bridge.h
│   │   ├── network.h
│   │   ├── protocol.h
│   │   ├── platform_compat.h
│   │   ├── json_parser.h
│   │   ├── tls_client.h
│   │   ├── webview_manager.h
│   │   ├── local_storage.h
│   │   └── updater.h
│   ├── src/                         # C/C++ 源代码
│   │   ├── network/                 # 网络通信层
│   │   │   ├── NetworkClient.h/.cpp
│   │   │   ├── TcpConnection.h/.cpp
│   │   │   ├── WebSocketClient.h/.cpp
│   │   │   ├── HttpConnection.h/.cpp
│   │   │   ├── TlsWrapper.h/.cpp
│   │   │   ├── tls_client.c
│   │   │   └── Sha1.h/.cpp
│   │   ├── security/                # 安全模块 (C++ → Rust FFI)
│   │   │   ├── CryptoEngine.h/.cpp
│   │   │   └── TokenManager.h/.cpp
│   │   ├── storage/                 # 本地存储
│   │   │   ├── LocalStorage.h/.cpp
│   │   │   └── SessionManager.h/.cpp
│   │   ├── app/                     # 应用外壳
│   │   │   ├── Main.cpp             # 入口函数
│   │   │   ├── AppContext.h/.cpp
│   │   │   ├── ClientHttpServer.h/.cpp
│   │   │   ├── IpcBridge.h/.cpp
│   │   │   ├── WebViewManager.h/.cpp
│   │   │   ├── TlsServerContext.h/.cpp
│   │   │   └── Updater.h/.cpp
│   │   ├── ai/                      # AI 多提供商
│   │   │   ├── AIProvider.h/.cpp
│   │   │   ├── OpenAIProvider.h/.cpp
│   │   │   ├── GeminiProvider.h/.cpp
│   │   │   ├── CustomProvider.h/.cpp
│   │   │   ├── AIConfig.h/.cpp
│   │   │   └── AIChatSession.h/.cpp
│   │   ├── plugin/                  # 插件系统
│   │   │   ├── types.h
│   │   │   ├── PluginInterface.h
│   │   │   ├── PluginManifest.h/.cpp
│   │   │   └── PluginManager.h/.cpp
│   │   └── util/                    # 工具函数
│   │       ├── Logger.h/.cpp
│   │       └── Utils.h/.cpp
│   ├── security/                    # Rust 安全模块
│   │   ├── Cargo.toml               # Rust 包配置
│   │   ├── build.rs                 # NASM 编译脚本
│   │   ├── src/
│   │   │   ├── lib.rs               # Rust 库入口 + FFI 导出
│   │   │   ├── crypto.rs            # E2E + ASM FFI 封装
│   │   │   ├── asm_bridge.rs        # Rust → ASM FFI 桥接
│   │   │   ├── sanitizer.rs         # 输入安全校验
│   │   │   ├── secure_storage.rs    # 安全存储
│   │   │   └── session.rs           # 会话管理
│   │   ├── asm/                     # NASM 汇编 (仅加密核心)
│   │   │   └── obfuscate.asm        # ChronoStream v1
│   │   └── include/
│   │       └── chrono_client_security.h  # C 头文件
│   ├── devtools/                    # 开发者工具
│   │   ├── cli/                     # 独立 CLI
│   │   │   ├── main.c
│   │   │   ├── devtools_cli.h
│   │   │   ├── net_http.c
│   │   │   ├── Makefile
│   │   │   └── commands/            # 30+ 命令模块
│   │   │       ├── init_commands.c
│   │   │       ├── cmd_health.c
│   │   │       ├── cmd_endpoint.c
│   │   │       ├── cmd_token.c
│   │   │       ├── cmd_ipc.c
│   │   │       ├── cmd_user.c
│   │   │       ├── cmd_session.c
│   │   │       ├── cmd_config.c
│   │   │       ├── cmd_storage.c
│   │   │       ├── cmd_crypto.c
│   │   │       ├── cmd_network.c
│   │   │       ├── cmd_ws.c
│   │   │       ├── cmd_msg.c
│   │   │       ├── cmd_friend.c
│   │   │       ├── cmd_db.c
│   │   │       ├── cmd_connect.c
│   │   │       ├── cmd_disconnect.c
│   │   │       ├── cmd_tls_info.c
│   │   │       ├── cmd_gen_cert.c
│   │   │       ├── cmd_json.c
│   │   │       ├── cmd_trace.c
│   │   │       ├── cmd_obfuscate.c
│   │   │       ├── cmd_ping.c
│   │   │       ├── cmd_watch.c
│   │   │       └── cmd_rate_test.c
│   │   └── core/                    # In-App DevTools (C++ + JS)
│   │       ├── DevToolsEngine.h/.cpp
│   │       ├── DevToolsHttpApi.h/.cpp
│   │       └── DevToolsIpcHandler.h/.cpp
│   ├── ui/                          # 前端 UI
│   │   ├── index.html               # 主页面
│   │   ├── oauth_callback.html      # OAuth 回调
│   │   ├── css/
│   │   │   ├── variables.css
│   │   │   ├── global.css
│   │   │   ├── main.css
│   │   │   ├── chat.css
│   │   │   ├── login.css
│   │   │   ├── community.css
│   │   │   ├── ai.css
│   │   │   ├── qq_group.css
│   │   │   └── themes/default.css
│   │   ├── js/
│   │   │   ├── app.js               # 应用主逻辑 (路由/设置)
│   │   │   ├── auth.js              # 登录/注册
│   │   │   ├── ipc.js               # IPC 通信
│   │   │   ├── api.js               # HTTP API 封装
│   │   │   ├── chat.js              # 聊天界面
│   │   │   ├── contacts.js          # 联系人管理
│   │   │   ├── community.js         # 社区页面
│   │   │   ├── oauth.js             # OAuth 登录
│   │   │   ├── utils.js             # 工具函数
│   │   │   ├── ai_chat.js           # AI 对话
│   │   │   ├── ai_smart_reply.js    # AI 智能回复
│   │   │   ├── plugin_api.js        # 插件 API
│   │   │   ├── qq_friends.js        # QQ 好友管理
│   │   │   ├── qq_group.js          # QQ 群管理
│   │   │   ├── qq_file.js           # 文件传输
│   │   │   ├── qq_emoji.js          # 表情包
│   │   │   ├── qq_status.js         # 在线状态
│   │   │   └── theme_engine.js      # 主题引擎
│   │   └── assets/images/
│   ├── tools/                       # 附加工具
│   │   ├── debug_cli.c              # 旧版调试 CLI
│   │   ├── stress_test.c            # 压力测试
│   │   └── Makefile
│   └── plugins/                     # 插件目录
│       ├── plugin_catalog.json
│       └── example_plugin/
├── docs/                            # 文档
│   ├── ASM_OBFUSCATION.md           # ChronoStream v1 算法文档
│   ├── PROJECT_OVERVIEW.md          # 项目全景概览 (本文档)
│   ├── BUILD.md                     # 构建指南
│   ├── AI_INTEGRATION.md            # AI 集成指南
│   ├── API.md                       # HTTP API 参考 (历史)
│   ├── PROTOCOL.md                  # 通信协议 (历史)
│   └── HTTPS_MIGRATION.md           # HTTPS 迁移记录
├── plans/                           # 计划文档
│   ├── phase_handover.md            # 项目交接文档
│   ├── ARCHITECTURE.md              # 架构文档
│   └── phase_*.md                   # Phase 1-12 + 专项计划
├── tests/                           # 测试脚本
│   ├── asm_obfuscation_test.sh
│   ├── api_verification_test.sh
│   ├── loopback_test.sh
│   └── security_pen_test.sh
├── installer/                       # NSIS 安装脚本
│   └── client_installer.nsi
├── reports/                         # 测试报告
│   ├── SUMMARY.md
│   └── asm_obfuscation_results.md
├── CMakeLists.txt                   # 根 CMake
└── Makefile                         # 根 Makefile (一键构建)
```

---

## 4. 模块详解

### 4.1 网络层

网络层负责客户端与服务端之间的所有通信，支持 **TCP**、**TLS**、**HTTP/HTTPS**、**WebSocket** 四种协议。

**核心文件**:
- [`client/src/network/TcpConnection.h/.cpp`](client/src/network/TcpConnection.h) — 底层 TCP 连接管理 (连接/断连/发送/接收)
- [`client/src/network/TlsWrapper.h/.cpp`](client/src/network/TlsWrapper.h) — OpenSSL TLS 封装 (SSL_connect/read/write)
- [`client/src/network/HttpConnection.h/.cpp`](client/src/network/HttpConnection.h) — HTTP 请求/响应解析 (GET/POST/PUT/DELETE)
- [`client/src/network/WebSocketClient.h/.cpp`](client/src/network/WebSocketClient.h) — WebSocket 客户端 (握手/帧编码/发送/接收)
- [`client/src/network/NetworkClient.h/.cpp`](client/src/network/NetworkClient.h) — 统一网络客户端接口
- [`client/src/network/tls_client.c`](client/src/network/tls_client.c) — C 版 TLS 客户端 (兼容旧代码)
- [`client/src/network/Sha1.h/.cpp`](client/src/network/Sha1.h) — SHA-1 实现 (WebSocket 握手指令)

**架构图**:
```
NetworkClient
  ├── TcpConnection (TCP 基础连接)
  │     └── TlsWrapper (OpenSSL TLS 加密)
  ├── HttpConnection (HTTP 请求/响应)
  ├── WebSocketClient (WebSocket 长连接)
  └── tls_client.c (C 版 TLS 兼容)
```

**数据流**:
```
[应用层] → NetworkClient::http_request()
         → TcpConnection::connect() + TlsWrapper::ssl_connect()
         → HttpConnection::send_request() / parse_response()
         → SSL_read/SSL_write (加密传输)
```

### 4.2 安全层

安全层由三部分组成：**C++ 统一接口** → **Rust FFI 调度** → **NASM 私有加密核心**。

#### 4.2.1 Rust 安全模块

[`client/security/`](client/security/Cargo.toml) 是一个独立的 Rust crate (`chrono_client_security`)，以 `staticlib + cdylib` 形式输出。

| 模块 | 文件 | 职责 |
|------|------|------|
| 库入口 | [`lib.rs`](client/security/src/lib.rs) | FFI 导出，注册所有模块 |
| E2E 加密 | [`crypto.rs`](client/security/src/crypto.rs) | AES-256-GCM 加密/解密，ASM 混淆封装 |
| ASM 桥接 | [`asm_bridge.rs`](client/security/src/asm_bridge.rs) | Rust → NASM FFI 桥接，4 个单元测试 |
| 安全存储 | [`secure_storage.rs`](client/security/src/secure_storage.rs) | AES-256 加密本地存储 |
| 输入校验 | [`sanitizer.rs`](client/security/src/sanitizer.rs) | 路径/文件名/用户名/Token/消息长度校验 |
| 会话管理 | [`session.rs`](client/security/src/session.rs) | 登录 Token 保存/读取/清除 |

#### 4.2.2 E2E 加密 (AES-256-GCM)

```rust
// 加密: 对方公钥 → 密文 (Base64)
char* rust_client_encrypt_e2e(plaintext, pubkey_b64);

// 解密: 己方私钥 → 明文
char* rust_client_decrypt_e2e(ciphertext_b64, privkey_b64);
```

#### 4.2.3 ASM 私有混淆 (ChronoStream v1)

详见 [`docs/ASM_OBFUSCATION.md`](docs/ASM_OBFUSCATION.md) — 包含完整的算法说明、函数接口、调试记录。

### 4.3 AI 模块

AI 模块支持 **6 种 AI 提供商**，通过统一的 C++ 接口和前端 JS 封装提供 AI 对话功能。

**核心文件**:
- [`client/src/ai/AIProvider.h/.cpp`](client/src/ai/AIProvider.h) — 抽象基类，定义 `chat()` / `chat_stream()` 接口
- [`client/src/ai/OpenAIProvider.h/.cpp`](client/src/ai/OpenAIProvider.h) — OpenAI 兼容协议 (OpenAI/DeepSeek/xAI/Ollama)
- [`client/src/ai/GeminiProvider.h/.cpp`](client/src/ai/GeminiProvider.h) — Google Gemini 协议实现
- [`client/src/ai/CustomProvider.h/.cpp`](client/src/ai/CustomProvider.h) — 用户自定义 API
- [`client/src/ai/AIConfig.h/.cpp`](client/src/ai/AIConfig.h) — 配置管理
- [`client/src/ai/AIChatSession.h/.cpp`](client/src/ai/AIChatSession.h) — 对话会话管理
- [`client/ui/js/ai_chat.js`](client/ui/js/ai_chat.js) — 前端 AI 对话逻辑
- [`client/ui/js/ai_smart_reply.js`](client/ui/js/ai_smart_reply.js) — AI 智能回复
- [`docs/AI_INTEGRATION.md`](docs/AI_INTEGRATION.md) — AI 集成详细文档

**提供商对比**:

| 提供商 | 协议 | 端点格式 | API Key 必需 |
|--------|------|----------|-------------|
| OpenAI | OpenAI 兼容 | `/v1/chat/completions` | ✅ |
| DeepSeek | OpenAI 兼容 | `/v1/chat/completions` | ✅ |
| xAI Grok | OpenAI 兼容 | `/v1/chat/completions` | ✅ |
| Ollama | OpenAI 兼容 | `/v1/chat/completions` | ❌ (本地) |
| Gemini | Google 原生 | `generateContent` | ✅ |
| 自定义 | OpenAI 兼容 | 用户指定 | 可选 |

**架构**:
```
AIChat (JavaScript)
  │ IPC / 设置保存
  ▼
C++ AIProvider (工厂模式)
  ├── OpenAIProvider (OpenAI / DeepSeek / xAI / Ollama)
  ├── GeminiProvider (Google Gemini 原生协议)
  └── CustomProvider (用户自定义端点)
      │ HTTP POST
      ▼
  目标 AI API 服务
```

### 4.4 插件系统

插件系统允许第三方开发者通过 **JSON manifest + JavaScript** 扩展客户端功能。

**核心文件**:
- [`client/src/plugin/types.h`](client/src/plugin/types.h) — 插件类型定义
- [`client/src/plugin/PluginManifest.h/.cpp`](client/src/plugin/PluginManifest.h) — manifest 解析与验证
- [`client/src/plugin/PluginManager.h/.cpp`](client/src/plugin/PluginManager.h) — 插件生命周期管理
- [`client/src/plugin/PluginInterface.h`](client/src/plugin/PluginInterface.h) — 插件接口抽象
- [`client/plugins/plugin_catalog.json`](client/plugins/plugin_catalog.json) — 已知插件目录
- [`client/plugins/example_plugin/`](client/plugins/example_plugin/) — 示例插件

**插件 manifest 示例** ([`client/plugins/example_plugin/manifest.json`](client/plugins/example_plugin/manifest.json)):
```json
{
  "id": "example-plugin",
  "name": "示例插件",
  "version": "1.0.0",
  "entry": "plugin.js",
  "permissions": ["ipc", "storage", "network"],
  "hooks": ["onMessage", "onUIReady"]
}
```

### 4.5 DevTools 开发者工具

DevTools 包含两个子系统：**独立 CLI** (C99) 和 **In-App DevTools** (C++ + JavaScript)。

#### 4.5.1 DevTools CLI

基于注册模式的命令行工具，支持交互模式和单命令模式。

**命令分类** (30+ 个命令):

| 分类 | 命令 | 用途 |
|------|------|------|
| 基础 | `health`, `endpoint`, `token`, `user` | 服务健康检查、端点配置、令牌管理 |
| 客户端本地 | `session`, `config`, `storage`, `crypto`, `network` | 本地状态管理 |
| WebSocket | `ws` | WebSocket 连接/发送/接收/监控 |
| 数据库 | `msg`, `friend`, `db` | 消息、好友、数据库操作 |
| 连接管理 | `connect`, `disconnect` | 服务器连接控制 |
| 安全诊断 | `tls`, `gen_cert`, `json`, `trace` | TLS 信息、证书生成、JSON 解析 |
| ASM 混淆 | `obfuscate` | ASM 密钥生成/加密/解密/测试 |
| 性能测试 | `ping`, `watch`, `rate_test` | 延迟、监控、QPS 测试 |

**使用示例**:
```bash
# 交互模式
chrono-devtools

# 单命令模式
chrono-devtools health
chrono-devtools obfuscate genkey
chrono-devtools obfuscate encrypt --data "Hello" --key <hex_key>
chrono-devtools ws connect --host 127.0.0.1 --port 4443
chrono-devtools crypto test
```

#### 4.5.2 In-App DevTools

嵌入在 Web UI 中的开发者面板，支持 IPC 消息监控、WebSocket 帧查看、网络请求追踪等功能。作为 Chrome 扩展兼容接口注册到插件系统。

**核心文件**:
- [`client/devtools/core/DevToolsEngine.cpp`](client/devtools/core/DevToolsEngine.cpp) — DevTools 引擎 (C++)
- [`client/devtools/core/DevToolsHttpApi.cpp`](client/devtools/core/DevToolsHttpApi.cpp) — HTTP API 处理
- [`client/devtools/core/DevToolsIpcHandler.cpp`](client/devtools/core/DevToolsIpcHandler.cpp) — IPC 拦截
- [`client/devtools/ui/js/devtools.js`](client/devtools/ui/js/devtools.js) — 前端 DevTools 面板

### 4.6 前端 UI

前端采用 **纯 JavaScript + HTML + CSS** 实现，通过 **IPC (HTTP + JSON)** 与 C++ 后端通信。

**页面结构** ([`client/ui/index.html`](client/ui/index.html)):
```
#app-container
├── #page-auth (登录/注册页面)
│   ├── #form-login (登录表单)
│   │   └── oauth-buttons (QQ / WeChat OAuth)
│   └── #form-register (注册表单)
│       └── email-register (邮箱验证)
└── #page-main (主界面)
    ├── .sidebar (侧栏)
    │   ├── user-avatar (用户头像)
    │   ├── nav-tabs (聊天/联系人/社区/群组)
    │   ├── external-links (Bilibili/AcFun/ComicExpo)
    │   └── sidebar-footer (设置按钮)
    └── .main-content (主内容区)
        ├── #view-chat (聊天视图)
        ├── #view-contacts (联系人视图)
        ├── #view-community (社区视图)
        ├── #view-groups (群组视图)
        └── #view-settings (设置视图)
```

**关键 JS 模块**:

| 模块 | 文件 | 行数 | 职责 |
|------|------|------|------|
| app.js | [`client/ui/js/app.js`](client/ui/js/app.js) | ~510 | 路由、设置、外部链接 |
| auth.js | [`client/ui/js/auth.js`](client/ui/js/auth.js) | 登录/注册逻辑 |
| ipc.js | [`client/ui/js/ipc.js`](client/ui/js/ipc.js) | IPC 通信 + 扩展注册 |
| api.js | [`client/ui/js/api.js`](client/ui/js/api.js) | HTTP API 封装 |
| chat.js | [`client/ui/js/chat.js`](client/ui/js/chat.js) | 聊天界面逻辑 |
| ai_chat.js | [`client/ui/js/ai_chat.js`](client/ui/js/ai_chat.js) | AI 对话 (6 提供商) |
| contacts.js | [`client/ui/js/contacts.js`](client/ui/js/contacts.js) | 联系人管理 |
| oauth.js | [`client/ui/js/oauth.js`](client/ui/js/oauth.js) | OAuth 登录 |

### 4.7 存储层

| 组件 | 文件 | 用途 |
|------|------|------|
| LocalStorage (C++) | [`client/src/storage/LocalStorage.h/.cpp`](client/src/storage/LocalStorage.h) | 本地键值存储 |
| SessionManager (C++) | [`client/src/storage/SessionManager.h/.cpp`](client/src/storage/SessionManager.h) | 登录会话状态管理 |
| Secure Storage (Rust) | [`client/security/src/secure_storage.rs`](client/security/src/secure_storage.rs) | AES-256 加密的安全存储 |
| Session (Rust) | [`client/security/src/session.rs`](client/security/src/session.rs) | Token 持久化 |

---

## 5. 数据流详解

### 5.1 消息发送流程

```
[用户输入消息 → 点击发送]
    │
    ▼
[WebUI: chat.js]
    → 构造消息 JSON
    → IPC.send(MessageType.SEND_MESSAGE, { content, receiver, ... })
    │
    ▼
[IPC: ipc.js → HTTP POST → ClientHttpServer]
    → JSON 序列化 → HTTP POST http://127.0.0.1:4321/ipc
    │
    ▼
[C++ ClientHttpServer::handle_ipc()]
    → 解析 JSON → 路由到消息处理
    → 调用 CryptoEngine::encrypt_message()
    │
    ▼
[CryptoEngine (C++)]
    → E2E: rust_client_encrypt_e2e()    [AES-256-GCM]
    → ASM: rust_client_obfuscate_message() [ChronoStream v1]
    │
    ▼
[Rust FFI → NASM]
    → crypto.rs: obfuscate_message()
    → asm_bridge.rs: obfuscate()
    → obfuscate.asm: ksa_init() + gen_keystream() + XOR
    │
    ▼
[密文返回 C++]
    → NetworkClient::ws_send() 或 http_post()
    → WebSocket 或 HTTPS → 服务端
    │
    ▼
[接收方]
    → WebSocket 收到密文
    → CryptoEngine::decrypt_message()
    → ASM 解密 + E2E 解密
    → 明文 → UI 显示
```

### 5.2 登录流程

```
[用户输入凭据 → 点击登录]
    │
    ▼
[WebUI: auth.js / oauth.js]
    → 构造登录请求
    → IPC.send(LOGIN, { username, password, ... })
    │
    ▼
[C++ ClientHttpServer]
    → HTTP POST /api/auth/login
    │
    ▼
[服务端验证]
    → 验证凭据
    → 返回 Token + 会话信息
    │
    ▼
[crypto.rs → session.rs]
    → 解析 Token
    → rust_session_save("token", token)
    → rust_session_save("user_info", JSON)
    → 生成 ASM 密钥 (用户身份派生)
    │
    ▼
[WebUI]
    → sessionStorage.setItem('chrono_user', ...)
    → 切换到主界面 (#page-main)
    → 加载联系人/聊天列表
```

### 5.3 AI 对话流程

```
[用户在 AI 面板输入提示词]
    │
    ▼
[WebUI: ai_chat.js]
    → 选择提供商 (OpenAI / DeepSeek / xAI / Ollama / Gemini / 自定义)
    → 读取配置 (API Key / 模型 / 端点)
    → AIChat.callAPI(messages, provider, config)
    │
    ▼
[AIChat.callAPI()]
    → 根据提供商选择请求格式:
    ├── OpenAI 兼容: POST /v1/chat/completions { model, messages, ... }
    └── Gemini: POST generateContent { contents, generationConfig, ... }
    │
    ▼
[HTTP 请求]
    → fetch(url, { method, headers, body })
    → 携带 API Key (Authorization: Bearer 或 x-goog-api-key)
    │
    ▼
[AI 服务响应]
    → 解析 JSON 响应
    ├── OpenAI 兼容: response.choices[0].message.content
    └── Gemini: response.candidates[0].content.parts[0].text
    │
    ▼
[UI 渲染]
    → 追加到对话历史
    → 渲染 Markdown/代码块
    → 流式输出 (SSE / 逐块显示)
```

---

## 6. 开发指南

### 6.1 添加新的 AI 提供商

**C++ 后端**:

1. 在 [`client/src/ai/`](client/src/ai/) 中创建 `NewProvider.h` 和 `NewProvider.cpp`
2. 继承 [`AIProvider`](client/src/ai/AIProvider.h:82) 基类，实现 `chat()` 和 `chat_stream()`
3. 在 [`AIProviderType`](client/src/ai/AIConfig.h:35) 枚举中添加新类型
4. 在 [`AIProvider.cpp`](client/src/ai/AIProvider.cpp) 的工厂方法中注册新提供商

**前端**:

5. 在 [`ai_chat.js`](client/ui/js/ai_chat.js:12) 的 `AIChat.PROVIDERS` 中添加新提供商配置
6. 在 [`ai_chat.js`](client/ui/js/ai_chat.js) 中实现 `callNewProviderAPI()` 方法
7. 在 [`index.html`](client/ui/index.html:276) 的 AI 设置下拉框中添加选项

**文档**:

8. 更新 [`docs/AI_INTEGRATION.md`](docs/AI_INTEGRATION.md)

### 6.2 创建新插件

1. 在 [`client/plugins/`](client/plugins/) 中创建插件目录，例如 `my-plugin/`
2. 创建 [`manifest.json`](client/plugins/example_plugin/manifest.json):
   ```json
   {
     "id": "my-plugin",
     "name": "我的插件",
     "version": "1.0.0",
     "entry": "plugin.js",
     "permissions": ["ipc"],
     "hooks": ["onMessage"]
   }
   ```
3. 创建 `plugin.js`，使用 [`ChronoExtensions`](client/ui/js/ipc.js:75) API 注册扩展点
4. (可选) 在 [`plugin_catalog.json`](client/plugins/plugin_catalog.json) 中注册

### 6.3 添加新 CLI 命令

1. 在 [`client/devtools/cli/commands/`](client/devtools/cli/commands/) 中创建 `cmd_xxx.c`
2. 实现 `int init_cmd_xxx(void)` 函数，调用 `register_command()`
3. 在 [`init_commands.c`](client/devtools/cli/commands/init_commands.c) 中添加 `extern int init_cmd_xxx(void);` 声明和调用
4. 重新编译: `cd client/devtools/cli && make`

**命令注册示例** ([`cmd_obfuscate.c`](client/devtools/cli/commands/cmd_obfuscate.c) 参考):
```c
int init_cmd_obfuscate(void) {
    Command cmd = { "obfuscate", cmd_obfuscate, 
                    "ASM 混淆加密测试", "obfuscate [genkey|encrypt|decrypt|test]" };
    register_command(&cmd);
    return 0;
}
```

### 6.4 修改前端 UI

前端 UI 使用 **纯 JavaScript**，无需前端构建工具。

- **样式修改**: [`client/ui/css/`](client/ui/css/) 目录下的 CSS 文件
- **页面结构**: [`client/ui/index.html`](client/ui/index.html)
- **主题**: [`client/ui/css/themes/default.css`](client/ui/css/themes/default.css) + [`theme_engine.js`](client/ui/js/theme_engine.js)
- **添加新页面**: 在 [`index.html`](client/ui/index.html) 的 `#app-container` 中添加新的 `.content-view`，在 [`app.js`](client/ui/js/app.js) 的 `switchTab()` 中注册

---

## 7. 测试指南

### 7.1 Rust 单元测试

```bash
cd client/security
cargo test --lib

# 预期输出: 4 个测试全部通过
# test test_different_keys ... ok
# test test_single_obfuscate ... ok
# test test_empty_data ... ok
# test test_obfuscate_deobfuscate_roundtrip ... ok
```

### 7.2 ASM 集成测试

```bash
bash tests/asm_obfuscation_test.sh
# 验证 P1-P8 所有文件完整性 + 代码模式 + 编译检查
```

### 7.3 API 验证测试

```bash
bash tests/api_verification_test.sh
# 验证 HTTP API 端点可用性
```

### 7.4 安全渗透测试

```bash
bash tests/security_pen_test.sh
# 路径穿越、XSS、SQL 注入等安全测试
```

### 7.5 回路测试

```bash
bash tests/loopback_test.sh
# 完整登录 → 发送消息 → 接收消息回路
```

---

## 8. 部署与发布

### 8.1 构建发布包

```bash
# 1. 编译所有组件
make all

# 2. 制作 NSIS 安装包
make installer
# 输出: installer/Chrono-shift-Setup.exe
```

### 8.2 安装包内容

```
Chrono-shift/
├── chrono-client.exe        # 桌面客户端
├── chrono-devtools.exe      # DevTools CLI
├── chrono_client_security.dll  # Rust 安全模块
├── ui/                      # 前端资源
│   ├── index.html
│   ├── css/
│   └── js/
├── certs/                   # TLS 证书
│   ├── server.crt
│   └── server.key
└── plugins/                 # 插件目录
    └── plugin_catalog.json
```

### 8.3 首次启动

1. 客户端自动生成自签名证书 (如不存在)
2. 初始化 Rust 安全模块 (`rust_client_init()`)
3. 启动本地 HTTP 服务器 (127.0.0.1:4321)
4. 打开 WebView2 窗口加载 `index.html`
5. 显示登录页面

---

## 9. 附录

### 9.1 关键文档索引

| 文档 | 内容 | 适合读者 |
|------|------|----------|
| [`README.md`](../README.md) | 项目入口、快速概览 | 所有开发者 |
| [`plans/phase_handover.md`](../plans/phase_handover.md) | 项目交接、Phase 状态、已知问题 | 新加入开发者 |
| [`plans/ARCHITECTURE.md`](../plans/ARCHITECTURE.md) | 详细架构说明、设计决策 | 架构师、高级开发者 |
| [`docs/ASM_OBFUSCATION.md`](ASM_OBFUSCATION.md) | ChronoStream v1 算法 | 安全工程师 |
| [`docs/AI_INTEGRATION.md`](AI_INTEGRATION.md) | AI 多提供商集成 | AI 功能开发者 |
| [`docs/BUILD.md`](BUILD.md) | 构建指南 | 所有开发者 |
| [`docs/PROJECT_OVERVIEW.md`](PROJECT_OVERVIEW.md) | 项目全景概览 (本文档) | 新开发者、评估者 |

### 9.2 已知问题 (S1-S11)

详见 [`plans/phase_handover.md`](../plans/phase_handover.md#4-已知问题清单) 的完整列表，包括：

| ID | 严重程度 | 简述 |
|----|----------|------|
| S1 | 🔴 严重 | CSP `unsafe-inline` 开启 |
| S2 | 🔴 严重 | Token 存入 localStorage |
| S3 | 🟡 中等 | JSON 注入漏洞 |
| S4 | 🟡 中等 | innerHTML XSS (contacts.js) |
| S5 | 🟡 中等 | innerHTML XSS (community.js) |
| S6 | 🟡 中等 | IPC 路由线性匹配 |
| S7 | 🟡 中等 | SSRF 风险 |
| S8-S10 | 🟢 低 | 多处 stub 未实现 |
| S11 | ℹ️ 信息 | oauth.js 递归调用 |

### 9.3 设计决策总结

| 决策 | 选择 | 理由 |
|------|------|------|
| 客户端语言 | C++17 | 性能 + 跨平台 + WebView2 集成 |
| 安全模块语言 | Rust | 内存安全 + FFI 兼容 + 生态丰富 |
| 私有加密语言 | NASM x64 | 抗逆向分析 + 性能极致 |
| 前端技术 | 纯 JS (无框架) | 零依赖 + 直接嵌入 WebView2 |
| 通信协议 | HTTP + JSON | 简单可靠，前端 fetch 原生支持 |
| 构建系统 | CMake + Cargo + Makefile | 多语言混合构建的最佳实践 |
| 插件架构 | JSON manifest + JS | 低门槛 + 安全沙箱 |
| AI 协议 | OpenAI 兼容优先 | 生态最成熟，提供商最多 |

---

*文档版本: v2.0.0 — 完成于 2026-05-03*
