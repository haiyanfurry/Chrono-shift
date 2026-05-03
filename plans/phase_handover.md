# Chrono-shift (墨竹) 项目交接文档

> **版本**: v2.0.0 | **日期**: 2026-05-03
> **项目**: Chrono-shift (墨竹) — QQ 风格跨平台即时通讯桌面客户端
> **许可证**: 详见 [`LICENSE`](LICENSE)

---

## 目录

1. [项目概览](#1-项目概览)
2. [全部 Phase 完成状态](#2-全部-phase-完成状态)
3. [模块详细说明](#3-模块详细说明)
4. [已知问题清单](#4-已知问题清单)
5. [遗留待办事项](#5-遗留待办事项)
6. [构建说明精简版](#6-构建说明精简版)
7. [测试运行指南](#7-测试运行指南)
8. [关键文件索引](#8-关键文件索引)

---

## 1. 项目概览

### 一句话定位

**Chrono-shift (墨竹)** 是一个纯客户端的 QQ 风格即时通讯桌面应用，内置 E2E 加密、ASM 私有混淆、AI 聊天和插件系统。

### 技术栈快照

| 层级 | 语言/技术 | 主要用途 |
|------|----------|---------|
| 客户端外壳 | C++17 | WebView2 集成、IPC 桥接、HTTP 服务器、网络通信 |
| 安全模块 | Rust (`client/security/`) | AES-256-GCM E2E 加密、安全存储、会话管理 |
| 汇编加密 | NASM x64 (Win64 COFF) | ChronoStream v1 私有对称流密码 |
| 前端 UI | HTML5 + CSS3 + JavaScript | QQ 风格 Web 界面 |
| CLI 工具 | C99 (WinSock2) | 调试 CLI、压力测试、DevTools 面板 |
| 构建系统 | CMake + Cargo + Makefile + NSIS | 多语言混合编译、安装包制作 |

### 关键数据

- **源代码总量**: 约 35,000+ 行（C++ ~8,000 + Rust ~500 + NASM ~200 + C ~12,000 + JS ~4,000 + CSS ~2,000）
- **客户端主程序**: 约 500 行 C++ (`Main.cpp`)
- **安全模块**: 4 个公开 FFI 函数 (`rust_client_init`, `rust_client_encrypt_e2e`, `rust_client_obfuscate`, `rust_client_deobfuscate`)
- **DevTools CLI**: 30+ 个命令，覆盖网络/安全/调试/性能测试
- **AI 提供商**: 6 家 (OpenAI/DeepSeek/xAI/Ollama/Gemini/Custom)
- **已知问题**: 11 项 (S1-S11)

---

## 2. 全部 Phase 完成状态

### 已完成 Phase (1-12 + D + 特殊计划)

| Phase | 名称 | 关键交付 | 备注 |
|-------|------|---------|------|
| 1 | 项目骨架 | 目录结构、Rust FFI、C 基础框架、HTML 结构 | 奠定项目基础 |
| 2 | 核心通信层 | HTTP/WebSocket 服务器、客户端网络层、协议定义 | TCP/TLS/WS 实现 |
| 3 | 用户系统 | 注册/登录、JWT 认证、个人信息、好友系统 | 认证体系 |
| 4 | 消息系统 | 一对一通讯、消息存储、在线状态 | 核心聊天功能 |
| 5 | 主题/模板系统 | 纯白默认主题、CSS 变量引擎、模板 CRUD | UI 可定制性 |
| 6 | UI QQ 风格重构 | QQ 风格 CSS (#12B7F5/#9EEA6A)、CLI 调试增强、安全测试自动化 | 视觉风格定型 |
| 7 | 安全加固 | CSRF/SSRF 防护、文件类型校验、路径遍历防护 | 服务端安全 |
| 8 | 安装包与发布 | NSIS 安装脚本、HTTPS 迁移、文档完善 | 可分发 |
| 9 | C++ 重构 + OAuth | 客户端 C 到 C++ 迁移、OAuth 登录、邮箱验证 | 代码质量 |
| 9-1b | 客户端 C++ 重构 | C 到 C++ 迁移、模块化重构 (`src/network/`, `src/storage/`, `src/util/`) | 子阶段 |
| 9-2 | OAuth 登录 | QQ/微信/OAuth/邮箱注册登录 | 子阶段 |
| 10 | Rust+ASM 混淆 | ChronoStream v1 私有加密、NASM 汇编核心 (`obfuscate.asm`) | **最新完成** |
| 11 | AI 多提供商 | 6 家 AI 提供商集成 (OpenAI/DeepSeek/xAI/Ollama/Gemini/Custom) | 智能化 |
| 12 | 综合扩展规划 | 插件系统、QQ 社交功能、DevTools（计划文档） | 📋 规划阶段 |
| D | 开发者工具 | DevTools 面板 (CLI 30+ 命令 + UI CSS/JS 实时调试) | 开发体验 |
| — | 服务端移除 | `server/` 目录移除，项目聚焦纯客户端架构 | 架构精简 |
| — | HTTPS 迁移 | 自签名证书生成、TLS 1.3 强制、gen_cert 工具 | 传输安全 |

### Phase 依赖关系图

```
Phase 1 (骨架) → Phase 2 (网络) → Phase 3 (用户) → Phase 4 (消息)
                                                         ↓
Phase 5 (主题) → Phase 6 (QQ UI) → Phase 7 (安全) → Phase 8 (安装包)
                                                         ↓
Phase 9 (C+++OAuth) → Phase 10 (ASM) → Phase 11 (AI) → Phase 12 (规划)
                                                         ↓
                                               Phase D (DevTools, 贯穿)
```

---

## 3. 模块详细说明

### 3.1 客户端 C++ 核心 (`client/src/`)

| 模块 | 路径 | 文件数 | 语言 | 功能 |
|------|------|--------|------|------|
| 网络层 | [`client/src/network/`](client/src/network/) | 8 | C++17 | TCP/TLS/HTTP/WebSocket 客户端实现 |
| 安全引擎 | [`client/src/security/`](client/src/security/) | 4 | C++17 | 调用 Rust FFI 完成 E2E 加密/ASM 混淆 |
| 存储层 | [`client/src/storage/`](client/src/storage/) | 4 | C++17 | LocalStorage (部分 stub)、SessionManager |
| 应用层 | [`client/src/app/`](client/src/app/) | 12 | C++17 | WebView2 管理器、IPC 桥接、HTTP 服务器、更新器 |
| AI 层 | [`client/src/ai/`](client/src/ai/) | 12 | C++17 | 6 家 AI 提供商集成、会话管理 |
| 插件层 | [`client/src/plugin/`](client/src/plugin/) | 8 | C++17 | 插件清单、管理器、接口定义 |
| 工具 | [`client/src/util/`](client/src/util/) | 4 | C++17 | 日志、字符串工具 |

### 3.2 Rust 安全模块 (`client/security/`)

| 文件 | 行数 | 功能 |
|------|------|------|
| [`lib.rs`](client/security/src/lib.rs) | 78 | 模块入口，4 个 `extern "C"` FFI 导出 |
| [`crypto.rs`](client/security/src/crypto.rs) | 253 | AES-256-GCM E2E 加密 + ASM 混淆消息包装 |
| [`asm_bridge.rs`](client/security/src/asm_bridge.rs) | 128 | NASM 汇编的 Rust FFI 桥接 + 4 个单元测试 |
| [`session.rs`](client/security/src/session.rs) | 93 | 会话管理 (save/get_token/is_logged_in/clear) |
| [`secure_storage.rs`](client/security/src/secure_storage.rs) | 118 | 设备密钥生成、AES 加密存储 |
| [`sanitizer.rs`](client/security/src/sanitizer.rs) | 68 | 输入校验 (路径/用户名/Token/消息长度) |

### 3.3 NASM 汇编 (`client/security/asm/`)

| 文件 | 说明 |
|------|------|
| [`obfuscate.asm`](client/security/asm/obfuscate.asm) | ChronoStream v1 算法 — ksa_init + gen_keystream + 主循环 |
| [`obfuscate.lst`](client/security/asm/obfuscate.lst) | NASM 汇编列表文件 |

### 3.4 前端 UI (`client/ui/`)

| 子目录 | 文件数 | 说明 |
|--------|--------|------|
| [`js/`](client/ui/js/) | 18 | IPC/API/Auth/Chat/Contacts/AI Chat 等 |
| [`css/`](client/ui/css/) | 8 | QQ 风格样式表 (含 themes/) |
| [`index.html`](client/ui/index.html) | 1 | 单页应用入口 |

### 3.5 开发者工具 (`client/devtools/`)

| 子模块 | 路径 | 说明 |
|--------|------|------|
| CLI 命令 | [`cli/commands/`](client/devtools/cli/commands/) | 30+ 个 C 命令文件 |
| CLI 主入口 | [`cli/main.c`](client/devtools/cli/main.c) | 命令行解析、Base64/Hex 编解码 |
| 核心引擎 | [`core/`](client/devtools/core/) | DevToolsEngine/HttpApi/IpcHandler (C++) |
| UI 面板 | [`ui/`](client/devtools/ui/) | JS 调试面板 + CSS 样式 |

### 3.6 AI 提供商一览

| 提供商 | 实现类 | 说明 |
|--------|--------|------|
| OpenAI | [`OpenAIProvider.cpp`](client/src/ai/OpenAIProvider.cpp) | 通用 OpenAI 兼容 API |
| DeepSeek | 复用 OpenAIProvider | 端点配置为 `api.deepseek.com` |
| xAI | 复用 OpenAIProvider | 端点配置为 `api.x.ai` |
| Ollama | 复用 OpenAIProvider | 本地 `localhost:11434` |
| Gemini | [`GeminiProvider.cpp`](client/src/ai/GeminiProvider.cpp) | Google Gemini 独立 API |
| Custom | [`CustomProvider.cpp`](client/src/ai/CustomProvider.cpp) | 用户自定义 API 端点 |

### 3.7 DevTools CLI 命令分类

| 分类 | 命令 | 数量 |
|------|------|------|
| 基础功能 | `health`, `endpoint`, `token`, `ipc`, `user` | 5 |
| 客户端本地 | `session`, `config`, `storage`, `crypto`, `network` | 5 |
| 网络调试 | `ws` (connect/send/recv/close/status) | 1 |
| 数据库操作 | `msg`, `friend`, `db` | 3 |
| 连接管理 | `connect`, `disconnect` | 2 |
| 安全与诊断 | `tls-info`, `gen-cert`, `json-parse`, `json-pretty`, `trace`, **`obfuscate`** | 6 |
| 性能测试 | `ping`, `watch`, `rate-test` | 3 |
| **合计** | | **25+** |

---

## 4. 已知问题清单

### 安全问题

| ID | 严重程度 | 描述 | 位置 | 建议修复 |
|----|---------|------|------|---------|
| S1 | 🔴严重 | CSP `unsafe-inline` + `unsafe-eval` 开启，XSS 攻击面大 | [`index.html:20`](client/ui/index.html#L20) | 改用 nonce 或 hash |
| S2 | 🔴严重 | Token 存入 `localStorage`，任何 XSS 即可窃取 | [`auth.js:28`](client/ui/js/auth.js#L28) | 改用 httpOnly cookie 或 WebView2 非持久存储 |
| S3 | 🟡中等 | `send_error_json` 未转义 `message` 参数 → JSON 注入 | [`ClientHttpServer.cpp:254`](client/src/app/ClientHttpServer.cpp#L254) | 对 message 做 JSON 转义 |
| S4 | 🟡中等 | `contacts.js` 中 `avatar_url` 直接插入 `innerHTML` 未转义 | [`contacts.js:39`](client/ui/js/contacts.js#L39) | 使用 `textContent` 或 `escapeHtml` |
| S5 | 🟡中等 | `contacts.js` 中 `last_message` 直接插入 `innerHTML` 未转义 | [`contacts.js:43`](client/ui/js/contacts.js#L43) | 使用 `textContent` 或 `escapeHtml` |
| S6 | 🟡中等 | IPC 路由错误：`handle_from_js` 总是调用第一个注册处理器，不按类型匹配 | [`IpcBridge.cpp:70`](client/src/app/IpcBridge.cpp#L70) | 用 `unordered_map` 按类型查找 |
| S7 | 🟡中等 | `community.js` 模板预览直接 fetch 外部 URL → SSRF 风险 | [`community.js:66`](client/ui/js/community.js#L66) | 限制模板 ID 为数字 |

### 空实现桩 (Stub)

| ID | 严重程度 | 描述 | 位置 | 建议修复 |
|----|---------|------|------|---------|
| S8 | 🟢低 | `LocalStorage` 的 `save_config`/`load_config`/`save_file`/`load_file` 均为空实现 | [`LocalStorage.cpp`](client/src/storage/LocalStorage.cpp) | 实现 JSON 文件读写 |
| S9 | 🟢低 | `WebViewManager::navigate` 和 `evaluate_script` 为空实现 | [`WebViewManager.cpp`](client/src/app/WebViewManager.cpp) | 集成 WebView2 真实实现 |
| S10 | 🟢低 | IPC `send_to_js` 为空实现 | [`IpcBridge.cpp:48`](client/src/app/IpcBridge.cpp#L48) | 实现 JS 消息推送 |

### 代码缺陷

| ID | 严重程度 | 描述 | 位置 | 建议修复 |
|----|---------|------|------|---------|
| S11 | ℹ️信息 | `oauth.js` 中 `Auth.sendEmailCode` 递归调用自身（死循环 bug） | [`oauth.js:115`](client/ui/js/oauth.js#L115) | 不应自调用 |

---

## 5. 遗留待办事项

### 高优先级

| # | 任务 | 相关 Issue | 预估工时 |
|---|------|-----------|---------|
| 1 | CSP 策略收紧 — 移除 `unsafe-inline`/`unsafe-eval` | S1 | 1d |
| 2 | Token 存储迁移 — 从 localStorage 改为 httpOnly cookie 或 WebView2 安全存储 | S2 | 2d |
| 3 | JSON 注入修复 — `send_error_json` 转义 | S3 | 0.5d |
| 4 | `innerHTML` 转义修复 — contacts.js | S4, S5 | 0.5d |
| 5 | IPC 路由精确匹配 — 按类型分发 | S6 | 1d |
| 6 | SSRF 修复 — community.js 模板预览 | S7 | 0.5d |
| 7 | `oauth.js` 递归 bug 修复 | S11 | 0.5d |
| 8 | LocalStorage 完整实现 | S8 | 2d |
| 9 | WebView2 真实集成 | S9 | 5d |

### 中优先级

| # | 任务 | 说明 | 预估工时 |
|---|------|------|---------|
| 10 | IPC `send_to_js` 实现 | 当前为空实现，JS 端无法接收服务端主动推送 | 1d |
| 11 | 插件系统完善 | 当前仅有框架定义和示例插件，需实现完整加载/沙箱/API | 3d |
| 12 | QQ 社交功能 | 好友分组、群组管理、文件传输、表情系统（QQ 完整功能） | 5d+ |

### 低优先级

| # | 任务 | 说明 | 预估工时 |
|---|------|------|---------|
| 13 | ai_smart_reply.js 集成 | 智能回复功能已定义但未完全集成到 UI | 1d |
| 14 | 主题引擎完善 | 当前仅 default.css，需支持主题切换 | 1d |
| 15 | 社区系统完善 | community.js 和 community.css 为基础框架 | 2d |

---

## 6. 构建说明精简版

### 6.1 依赖安装

| 依赖 | 版本要求 | 安装方式 (Windows) |
|------|---------|-------------------|
| MinGW-w64 | GCC ≥ 8.0 | `mingw32-make` / MSYS2 `pacman -S mingw-w64-x86_64-gcc` |
| Rust | ≥ 1.70 | `rustup-init.exe` + `rustup target add x86_64-pc-windows-gnu` |
| NASM | ≥ 2.15 | `nasm` 加入 PATH |
| CMake | ≥ 3.20 | `cmake` 加入 PATH |
| OpenSSL | ≥ 1.1 | MSYS2 `pacman -S mingw-w64-x86_64-openssl` |
| WebView2 | Win10+ 内置 | 可选，用于完整 UI 运行 |

### 6.2 编译步骤

```bash
# 1. 编译 Rust 安全模块 (含 ASM)
cd client/security
cargo build --release
# 输出: client/security/target/x86_64-pc-windows-gnu/release/chrono_client_security.a

# 2. 编译 DevTools CLI
cd client/devtools/cli
mingw32-make
# 输出: client/devtools/cli/out/chrono-devtools.exe

# 3. 编译客户端主程序
cd client
cmake -B build -G "MinGW Makefiles"
cmake --build build
```

### 6.3 快速测试

```bash
# Rust 单元测试 (含 ASM 混淆)
cd client/security && cargo test

# ASM 集成测试
bash tests/asm_obfuscation_test.sh
```

---

## 7. 测试运行指南

### 7.1 单元测试

| 测试类型 | 命令 | 覆盖模块 |
|---------|------|---------|
| Rust 单元测试 | `cd client/security && cargo test` | ASM 桥接 4 个测试 (roundtrip/空数据/单字节/不同密钥) |
| ASM 集成测试 | `bash tests/asm_obfuscation_test.sh` | 8 项验证 (NASM 编译/FFI/加密/CLI) |

### 7.2 测试脚本

| 脚本 | 用途 | 当前状态 |
|------|------|---------|
| [`tests/asm_obfuscation_test.sh`](tests/asm_obfuscation_test.sh) | ASM 混淆集成验证 | ✅ 8/8 通过 |
| [`tests/security_pen_test.sh`](tests/security_pen_test.sh) | 安全渗透测试 (10 类) | ✅ 需服务端配合 |
| [`tests/api_verification_test.sh`](tests/api_verification_test.sh) | API 接口验证 | ✅ 需服务端配合 |
| [`tests/loopback_test.sh`](tests/loopback_test.sh) | 端到端回环测试 (14 步) | ✅ 需服务端配合 |

### 7.3 ASM 测试 (无服务端依赖)

```bash
# 方法 1: Rust cargo test (最快)
cd client/security && cargo test

# 方法 2: 集成测试脚本
bash tests/asm_obfuscation_test.sh

# 方法 3: DevTools CLI 手动测试
client/devtools/cli/out/chrono-devtools.exe obfuscate genkey
client/devtools/cli/out/chrono-devtools.exe obfuscate encrypt --key <hex_key> --data "Hello World"
client/devtools/cli/out/chrono-devtools.exe obfuscate decrypt --key <hex_key> --data <ciphertext_b64>
```

---

## 8. 关键文件索引

### 配置文件

| 文件 | 说明 |
|------|------|
| [`CMakeLists.txt`](CMakeLists.txt) | 根 CMake 配置 |
| [`client/CMakeLists.txt`](client/CMakeLists.txt) | 客户端 CMake 配置 |
| [`client/security/Cargo.toml`](client/security/Cargo.toml) | Rust 安全模块配置 |
| [`client/devtools/cli/Makefile`](client/devtools/cli/Makefile) | DevTools CLI Makefile |
| [`Makefile`](Makefile) | 根 Makefile |
| [`.gitignore`](.gitignore) | Git 忽略规则 |

### 核心源文件

| 文件 | 行数 | 说明 |
|------|------|------|
| [`client/src/app/Main.cpp`](client/src/app/Main.cpp) | ~52 | 客户端入口点 (WinMain/main) |
| [`client/src/app/AppContext.cpp`](client/src/app/AppContext.cpp) | ~252 | 应用上下文、初始化流程 |
| [`client/src/app/IpcBridge.cpp`](client/src/app/IpcBridge.cpp) | ~129 | IPC 桥接 (JS ↔ C++) |
| [`client/src/app/ClientHttpServer.cpp`](client/src/app/ClientHttpServer.cpp) | ~531 | 内部 HTTP API 服务器 |
| [`client/src/app/WebViewManager.cpp`](client/src/app/WebViewManager.cpp) | ~212 | WebView2 管理器 (含 stub) |
| [`client/src/network/NetworkClient.cpp`](client/src/network/NetworkClient.cpp) | ~214 | 网络客户端外观模式 |
| [`client/src/network/TcpConnection.cpp`](client/src/network/TcpConnection.cpp) | ~361 | TCP 连接实现 |
| [`client/src/network/WebSocketClient.cpp`](client/src/network/WebSocketClient.cpp) | ~375 | WebSocket 客户端 |
| [`client/src/network/HttpConnection.cpp`](client/src/network/HttpConnection.cpp) | ~250 | HTTP 连接实现 |
| [`client/src/ai/AIProvider.cpp`](client/src/ai/AIProvider.cpp) | ~50 | AI 提供商标识解析 |
| [`client/src/ai/OpenAIProvider.cpp`](client/src/ai/OpenAIProvider.cpp) | ~273 | OpenAI 兼容 API 实现 |
| [`client/src/ai/GeminiProvider.cpp`](client/src/ai/GeminiProvider.cpp) | ~150+ | Gemini API 实现 |
| [`client/src/ai/CustomProvider.cpp`](client/src/ai/CustomProvider.cpp) | ~197 | 自定义 API 实现 |
| [`client/security/src/lib.rs`](client/security/src/lib.rs) | 78 | Rust 安全模块入口 |
| [`client/security/src/crypto.rs`](client/security/src/crypto.rs) | 253 | E2E 加密 + ASM 混淆 |
| [`client/security/src/asm_bridge.rs`](client/security/src/asm_bridge.rs) | 128 | ASM FFI 桥接 + 单元测试 |
| [`client/security/asm/obfuscate.asm`](client/security/asm/obfuscate.asm) | ~200 | ChronoStream v1 NASM 实现 |
| [`client/devtools/cli/main.c`](client/devtools/cli/main.c) | ~230+ | DevTools CLI 主入口 |

### 前端文件

| 文件 | 说明 |
|------|------|
| [`client/ui/index.html`](client/ui/index.html) | 单页应用入口 (CSP + 所有 JS/CSS 引用) |
| [`client/ui/js/ipc.js`](client/ui/js/ipc.js) | IPC 通信 + 插件扩展系统 |
| [`client/ui/js/api.js`](client/ui/js/api.js) | 后端 API 请求封装 |
| [`client/ui/js/auth.js`](client/ui/js/auth.js) | 登录/注册 |
| [`client/ui/js/chat.js`](client/ui/js/chat.js) | 聊天界面逻辑 |
| [`client/ui/js/ai_chat.js`](client/ui/js/ai_chat.js) | AI 聊天 (6 提供商) |
| [`client/ui/js/oauth.js`](client/ui/js/oauth.js) | OAuth 登录 (含 S11 bug) |
| [`client/ui/js/plugin_api.js`](client/ui/js/plugin_api.js) | 插件 API 框架 |

### 文档索引

| 文件 | 版本 | 说明 |
|------|------|------|
| [`README.md`](README.md) | **v2.0.0** | 项目入口文档 |
| [`plans/ARCHITECTURE.md`](plans/ARCHITECTURE.md) | **v2.0.0** | 架构设计文档 |
| [`docs/BUILD.md`](docs/BUILD.md) | **v2.0.0** | 构建指南 |
| [`docs/ASM_OBFUSCATION.md`](docs/ASM_OBFUSCATION.md) | **v1.0** | ChronoStream v1 算法文档 |
| [`docs/PROJECT_OVERVIEW.md`](docs/PROJECT_OVERVIEW.md) | **v1.0** | 综合项目说明 |
| [`docs/AI_INTEGRATION.md`](docs/AI_INTEGRATION.md) | v0.1 | AI 集成说明 |
| [`docs/HTTPS_MIGRATION.md`](docs/HTTPS_MIGRATION.md) | v0.5 | HTTPS 迁移记录 |
| [`CONTRIBUTING.md`](CONTRIBUTING.md) | v0.1 | 贡献指南 |
| [`reports/SUMMARY.md`](reports/SUMMARY.md) | **v2.0.0** | 综合测试报告 |
| [`reports/asm_obfuscation_results.md`](reports/asm_obfuscation_results.md) | v1.0 | ASM 测试报告 |

### 安装包

| 文件 | 说明 |
|------|------|
| [`installer/client_installer.nsi`](installer/client_installer.nsi) | 客户端 NSIS 安装脚本 |

---

> **交接说明**: 本文档为新开发者/维护者提供项目全景。建议先阅读 [`README.md`](README.md) 了解项目概况，再根据 [`docs/PROJECT_OVERVIEW.md`](docs/PROJECT_OVERVIEW.md) 深入各模块细节。如需构建和测试，参考 [`docs/BUILD.md`](docs/BUILD.md)。所有 Phase 状态详见 [`plans/ARCHITECTURE.md`](plans/ARCHITECTURE.md)。
