# Chrono-shift (墨竹) — QQ 风格跨平台即时通讯桌面客户端

> 跨平台即时通讯客户端 | AES-256-GCM E2E 加密 | ChronoStream v1 ASM 私有混淆 | AI 多提供商集成 | 插件系统 | DevTools

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)
![Language: C++17 + Rust + NASM + JavaScript](https://img.shields.io/badge/language-C%2B%2B%20%7C%20Rust%20%7C%20NASM%20%7C%20JavaScript-blue)
![Platform: Windows + Linux](https://img.shields.io/badge/platform-Windows%20%7C%20Linux-lightgrey)
![Version: v2.0.0](https://img.shields.io/badge/version-v2.0.0-green)

---

## 目录

- [项目概述](#项目概述)
- [技术架构](#技术架构)
- [功能特性](#功能特性)
- [快速开始](#快速开始)
- [项目结构](#项目结构)
- [CLI 调试工具](#cli-调试工具)
- [测试](#测试)
- [安装包制作](#安装包制作)
- [开源协议](#开源协议)
- [联系方式](#联系方式)

---

## 项目概述

**Chrono-shift (墨竹)** 是一个纯客户端架构的 QQ 风格即时通讯桌面应用，使用 **C++17** (客户端外壳) + **Web 技术** (QQ 风格 UI) + **Rust** (安全模块) + **NASM** (私有加密) 构建。

### 核心能力

| 能力 | 说明 |
|------|------|
| **即时通讯** | 一对一聊天、消息历史、在线状态 |
| **E2E 加密** | AES-256-GCM 端到端加密通信 |
| **ASM 私有混淆** | ChronoStream v1 — 自研 NASM 汇编对称流密码 (512 位密钥) |
| **AI 集成** | 6 家 AI 提供商 (OpenAI/DeepSeek/xAI/Ollama/Gemini/Custom) |
| **插件系统** | 基于 manifest.json 的插件扩展框架 |
| **QQ 风格 UI** | 纯白背景 · #12B7F5 蓝色主色调 · #9EEA6A 绿色气泡 |
| **开发者工具** | 30+ CLI 命令 + UI 调试面板 |

### 版本演进

| 版本 | 说明 |
|------|------|
| **v2.0.0** (当前) | AI 多提供商 + ASM 混淆 + DevTools + 插件系统 |
| v1.0.0 | C++ 重构 + OAuth 登录 + QQ 风格 UI 定型 |
| v0.3.0 | 服务端移除，纯客户端架构 |
| v0.2.0 | 安全加固 + HTTPS 迁移 + NSIS 安装包 |
| v0.1.0 | 初始版本 (C99 服务端 + 客户端) |

---

## 技术架构

```
┌──────────────────────────────────────────────────────────────┐
│                   QQ 风格 Web UI (ui/)                        │
│   18 JS 模块 · 8 CSS 样式表 · 单页应用 · 主题引擎             │
│   纯白背景 · #12B7F5 蓝色 · #9EEA6A 绿色气泡 · 280px 侧边栏   │
├──────────────────────────────────────────────────────────────┤
│               IPC Bridge (C++ ↔ JS)                          │
│               10 种消息类型 (0x01-0x50)                       │
│               WebSocket 二进制帧                              │
├──────────────────────────────────────────────────────────────┤
│          客户端应用层 (C++17 - src/app/)                      │
│  Main.cpp · AppContext · ClientHttpServer                    │
│  WebViewManager · IpcBridge · TlsServerContext · Updater     │
├──────────────────────────────────────────────────────────────┤
│          网络层 (C++17 - src/network/)                        │
│  TcpConnection · TlsWrapper · HttpConnection                 │
│  WebSocketClient · NetworkClient (外观模式)                   │
├──────────────┬───────────────────────────────────────────────┤
│  安全引擎     │    AI 集成层 (src/ai/)                        │
│  (src/security)│  OpenAIProvider (OpenAI/DS/xAI/Ollama)      │
│  CryptoEngine │  GeminiProvider · CustomProvider             │
│  TokenManager │  AIChatSession · AIProvider 工厂              │
├──────────────┴───────────────────────────────────────────────┤
│   Rust 安全模块 (client/security/)           NASM 汇编        │
│   crypto.rs · session.rs                     obfuscate.asm   │
│   secure_storage.rs · sanitizer.rs           ChronoStream v1 │
│   asm_bridge.rs (NASM FFI 桥接)             512-bit 密钥      │
├──────────────────────────────────────────────────────────────┤
│   插件系统 (src/plugin/)    │   DevTools (devtools/)          │
│   PluginManager             │   CLI 30+ 命令                  │
│   PluginManifest · 接口定义  │   UI 调试面板 · HTTP API       │
├──────────────────────────────────────────────────────────────┤
│               CLI 工具 (client/tools/)                        │
│               debug_cli · stress_test                        │
└──────────────────────────────────────────────────────────────┘
```

### 核心组件

| 组件 | 路径 | 语言 | 行数 | 说明 |
|------|------|------|------|------|
| 应用外壳 | [`client/src/app/`](client/src/app/) | C++17 | ~1,200 | Main/AppContext/IPC/WebView/HTTP Server |
| 网络通信 | [`client/src/network/`](client/src/network/) | C++17 | ~1,500 | TCP/TLS/HTTP/WebSocket 客户端 |
| 安全引擎 | [`client/src/security/`](client/src/security/) | C++17 | ~300 | CryptoEngine + TokenManager |
| AI 集成 | [`client/src/ai/`](client/src/ai/) | C++17 | ~1,000 | 6 提供商 + 会话管理 |
| 插件系统 | [`client/src/plugin/`](client/src/plugin/) | C++17 | ~400 | 管理器 + 清单 + 接口 |
| 本地存储 | [`client/src/storage/`](client/src/storage/) | C++17 | ~300 | LocalStorage + SessionManager |
| Rust 安全 | [`client/security/`](client/security/) | Rust | ~500 | AES-256-GCM + ASM FFI |
| NASM 汇编 | [`client/security/asm/`](client/security/asm/) | NASM | ~200 | ChronoStream v1 算法 |
| DevTools CLI | [`client/devtools/cli/`](client/devtools/cli/) | C99 | ~2,000 | 30+ 调试命令 |
| 前端 UI | [`client/ui/`](client/ui/) | HTML+CSS+JS | ~4,000 | QQ 风格界面 |

---

## 功能特性

### 🔐 安全特性

| 类别 | 措施 | 实现位置 |
|------|------|---------|
| **传输加密** | TLS 1.3 | [`TlsWrapper.cpp`](client/src/network/TlsWrapper.cpp) — OpenSSL |
| **E2E 加密** | AES-256-GCM 端到端加密 | [`crypto.rs`](client/security/src/crypto.rs) — Rust FFI |
| **私有混淆** | ChronoStream v1 (3-pass Fisher-Yates KSA + 8 级级联状态) | [`obfuscate.asm`](client/security/asm/obfuscate.asm) — NASM x64 |
| **安全存储** | AES-256-GCM 加密本地存储 | [`secure_storage.rs`](client/security/src/secure_storage.rs) — Rust FFI |
| **会话管理** | 令牌保存/验证/清除 | [`session.rs`](client/security/src/session.rs) — Rust FFI |

### 🤖 AI 集成

| 功能 | 说明 | 支持提供商 |
|------|------|-----------|
| **AI 聊天** | 在聊天面板中与 AI 对话 | ✅ 全部 6 家 |
| **智能回复** | 根据消息生成回复建议 | ✅ 全部 6 家 |
| **连接测试** | 验证 API 配置是否有效 | ✅ 全部 6 家 |
| **预设自动填充** | 选择提供商后自动填入端点/模型 | ✅ 6 种预设 |
| **无 Key 模式** | Ollama 本地运行无需 API Key | ✅ Ollama |

**AI 提供商详情:**

| 提供商 | 实现类 | API 端点 | 认证 |
|--------|--------|---------|------|
| OpenAI | [`OpenAIProvider`](client/src/ai/OpenAIProvider.cpp) | `api.openai.com` | API Key |
| DeepSeek | 复用 OpenAIProvider | `api.deepseek.com` | API Key |
| xAI | 复用 OpenAIProvider | `api.x.ai` | API Key |
| Ollama | 复用 OpenAIProvider | `localhost:11434` | 无 |
| Gemini | [`GeminiProvider`](client/src/ai/GeminiProvider.cpp) | `generativelanguage.googleapis.com` | API Key |
| Custom | [`CustomProvider`](client/src/ai/CustomProvider.cpp) | 用户指定 | 自定义 |

### 🧩 插件系统

| 组件 | 文件 | 说明 |
|------|------|------|
| 插件管理器 | [`PluginManager.cpp`](client/src/plugin/PluginManager.cpp) | 加载/卸载/枚举 |
| 插件清单 | [`PluginManifest.cpp`](client/src/plugin/PluginManifest.cpp) | `manifest.json` 解析 |
| 插件接口 | [`PluginInterface.h`](client/src/plugin/PluginInterface.h) | 标准 API |
| 示例插件 | [`example_plugin/`](client/plugins/example_plugin/) | 最小示例 |

### 🛠 CLI 调试工具

参见下方 [CLI 调试工具](#cli-调试工具) 章节。

### 🔒 ChronoStream v1 ASM 私有混淆

| 特性 | 值 |
|------|-----|
| 算法类型 | 自研对称流密码 |
| 密钥长度 | 512 位 (64 字节) |
| 实现语言 | NASM x64 (Win64 COFF) |
| KSA 算法 | 3-pass Fisher-Yates shuffle |
| 状态大小 | 8 字节级联状态 |
| 密钥流生成 | S-box swap + 级联更新 |
| 集成方式 | Rust build.rs → NASM → Rust FFI → C++ |
| 测试状态 | ✅ 4/4 cargo test 通过 |

详见 [`docs/ASM_OBFUSCATION.md`](docs/ASM_OBFUSCATION.md)。

---

## 快速开始

### 环境要求

**编译工具链**

| 平台 | 编译器 | 依赖 |
|------|--------|------|
| **Linux** | GCC ≥ 8, G++ ≥ 8 | `libssl-dev`, `libwebkit2gtk-4.1-dev`, `nasm` |
| **Windows** | MinGW-w64 GCC | WinSock2 (`-lws2_32`), OpenSSL DLL, `nasm` |

**必需依赖**
- **OpenSSL** (≥ 1.1.0): TLS 加密传输
- **NASM** (≥ 2.15): ASM 混淆模块编译

**可选依赖**
- **Rust** (≥ 1.70): 编译安全模块 (AES-256-GCM + ASM 桥接)
- **CMake** (≥ 3.20): 客户端主程序构建
- **NSIS** (≥ 3.0): 制作 Windows 安装包

### 编译构建

**1. Rust 安全模块 (含 ASM 编译)**

```bash
cd client/security
cargo build --release
# 输出: target/release/chrono_client_security.a/.dll
# 此步骤会自动编译 NASM → COFF → 链接到 Rust 静态库
```

**2. DevTools CLI**

```bash
# Windows (MinGW)
cd client/devtools/cli
mingw32-make
# 输出: out/chrono-devtools.exe
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
├── client/                              # 桌面客户端
│   ├── include/                         # C 头文件 (10 个)
│   ├── src/                             # C++17 源码
│   │   ├── app/                         # 应用外壳 (12 文件)
│   │   ├── network/                     # 网络通信 (8 文件)
│   │   ├── security/                    # 安全引擎 (4 文件)
│   │   ├── storage/                     # 本地存储 (4 文件)
│   │   ├── util/                        # 工具 (4 文件)
│   │   ├── ai/                          # AI 集成 (12 文件)
│   │   └── plugin/                      # 插件系统 (8 文件)
│   ├── security/                        # Rust 安全模块
│   │   ├── asm/                         # NASM 汇编 (obfuscate.asm)
│   │   ├── src/                         # Rust 源码 (6 文件)
│   │   └── include/                     # C FFI 头文件
│   ├── devtools/                        # 开发者工具
│   │   ├── cli/commands/                # 30+ CLI 命令
│   │   ├── core/                        # C++ 引擎
│   │   └── ui/                          # 调试面板
│   ├── tools/                           # 遗留 CLI 工具
│   ├── ui/                              # 前端 UI
│   │   ├── js/                          # 18 JS 模块
│   │   ├── css/                         # 8 样式表
│   │   └── assets/                      # 资源文件
│   └── plugins/                         # 插件示例
├── tests/                               # 测试脚本 (4 个)
├── installer/                           # NSIS 安装脚本
├── docs/                                # 文档
│   ├── BUILD.md                         # 构建指南
│   ├── ASM_OBFUSCATION.md               # ChronoStream v1 算法
│   ├── PROJECT_OVERVIEW.md              # 综合项目说明
│   └── AI_INTEGRATION.md                # AI 集成指南
├── plans/                               # 规划文档
│   ├── ARCHITECTURE.md                  # 架构设计
│   ├── phase_handover.md                # 项目交接
│   └── phase_*.md                       # 各阶段计划
├── reports/                             # 测试报告
│   ├── SUMMARY.md                       # 综合测试报告
│   └── asm_obfuscation_results.md       # ASM 测试报告
├── CMakeLists.txt / Makefile            # 构建配置
└── README.md                            # 本文件
```

---

## CLI 调试工具

### DevTools CLI (推荐)

位于 [`client/devtools/cli/`](client/devtools/cli/)，30+ 命令，支持交互模式和单命令模式。

| 分类 | 命令 | 说明 |
|------|------|------|
| **基础功能** | `health`, `endpoint`, `token`, `ipc`, `user` | 服务器通信 |
| **客户端本地** | `session`, `config`, `storage`, `crypto`, `network` | 本地诊断 |
| **网络调试** | `ws` (connect/send/recv/close/status/monitor) | WebSocket 调试 |
| **数据库操作** | `msg`, `friend`, `db` | 数据浏览 |
| **连接管理** | `connect`, `disconnect` | TCP 连接 |
| **安全与诊断** | `tls-info`, `gen-cert`, `json-parse`, `json-pretty`, `trace`, **`obfuscate`** | 安全工具 |
| **性能测试** | `ping`, `watch`, `rate-test` | 性能评估 |

**ASM 混淆命令 (新增):**

```bash
# 生成 512 位随机密钥
chrono-devtools obfuscate genkey

# 加密数据 (Base64 输出)
chrono-devtools obfuscate encrypt --key <hex_key> --data "Hello World"

# 解密数据
chrono-devtools obfuscate decrypt --key <hex_key> --data <ciphertext_b64>

# 测试
chrono-devtools obfuscate encrypt --key <hex_key> --data "test" | \
  chrono-devtools obfuscate decrypt --key <hex_key>
```

**使用示例:**

```bash
# 交互模式
client/devtools/cli/out/chrono-devtools.exe

# 单命令模式
client/devtools/cli/out/chrono-devtools.exe health
client/devtools/cli/out/chrono-devtools.exe "session show"
client/devtools/cli/out/chrono-devtools.exe "crypto test"
client/devtools/cli/out/chrono-devtools.exe "obfuscate genkey"
```

### 遗留 CLI 工具

位于 [`client/tools/`](client/tools/)：

```bash
# debug_cli — 调试接口
gcc -std=c99 -Wall -I../include debug_cli.c -o debug_cli -lws2_32 -lssl -lcrypto

# stress_test — 压力测试
gcc -std=c99 -Wall -I../include stress_test.c -o stress_test -lws2_32 -lm
```

---

## 测试

### Rust 单元测试 (ASM 混淆)

```bash
cd client/security && cargo test
```

4 个测试用例:
| 测试 | 说明 |
|------|------|
| `test_obfuscate_deobfuscate_roundtrip` | 完整加解密往返 |
| `test_empty_data` | 空数据处理 |
| `test_single_obfuscate` | 单字节加密 |
| `test_different_keys` | 不同密钥输出不同密文 |

### ASM 集成测试

```bash
bash tests/asm_obfuscation_test.sh
```

8 项验证: NASM 编译 / Rust FFI / 加密 / CLI 命令

### Shell 测试脚本 (需服务端)

| 脚本 | 覆盖范围 | 测试数 |
|------|---------|--------|
| [`security_pen_test.sh`](tests/security_pen_test.sh) | SQL注入/XSS/路径遍历/JWT/CSRF/SSRF/HTTPS | 30+ |
| [`api_verification_test.sh`](tests/api_verification_test.sh) | 用户/消息/文件/好友/模板 | 20+ |
| [`loopback_test.sh`](tests/loopback_test.sh) | 全链路 14 步 | 14 |

---

## 安装包制作

```bash
# 需要安装 NSIS (≥ 3.0)
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
- **感谢对象**： [Александра我喜欢你]
---

> **Chrono-shift (墨竹)** — 跨越时间的连接
