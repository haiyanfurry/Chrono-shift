# Chrono-shift v3.0.0

**纯 CLI I2P 即时通讯软件** — C++23 + Rust + NASM

## 概述

Chrono-shift 是一个基于 I2P 匿名网络的命令行即时通讯客户端。支持端到端加密 (AES-256-GCM)、好友管理、即时消息。

## 功能

- **Tor 匿名通信 (默认)**: 通过 SOCKS5 代理接入 Tor 网络，支持 ControlPort 管理
- **I2P 匿名通信 (备选)**: 通过 SAM v3 API 接入 I2P 网络，自动 NAT 穿透
- **E2E 加密**: AES-256-GCM 端到端加密 (Rust FFI)
- **好友系统**: 握手协议 (请求/接受/拒绝)，30 分钟临时屏蔽
- **即时消息**: CLI 交互式聊天模式
- **CLI 命令**: 22 个开发者工具命令 (网络诊断、加密测试、IPC 调试等)
- **ChronoStream v1**: NASM 实现的专有混淆加密算法
- **AI 集成**: 支持 OpenAI / DeepSeek / Gemini / 自定义 AI 提供商

## 快速开始

### 编译

```bash
# 要求: GCC 13+ / Clang 16+, CMake 3.20+, Rust, OpenSSL, NASM
cd client
cmake -B build -G "MinGW Makefiles"
cmake --build build
./build/chrono-client.exe
```

### 使用 (本地模式)

```bash
./chrono-client.exe
devtools> help          # 查看所有命令
devtools> health        # 检查服务器状态
devtools> crypto test   # 测试加密
devtools> config show   # 查看配置
```

### 使用 (I2P 模式)

```bash
# 1. 启动 I2P 路由器
cd /path/to/i2p && java -jar i2p.jar &

# 2. 启动 Chrono-shift
./chrono-client.exe
devtools> i2p start
[+] Connected to I2P SAM bridge
[+] Your I2P address: abc...xyz.b32.i2p

devtools> uid set alice
devtools> friend add bob@bobs-address.b32.i2p
devtools> msg chat bob
[bob]> hello!
```

## 项目结构

```
Chrono-shift/
├── client/                 # CLI 客户端源码
│   ├── src/
│   │   ├── ai/             # AI 提供商
│   │   ├── i2p/            # I2P SAM 客户端 (备选)
│   │   ├── tor/            # Tor SOCKS5 客户端 (默认)
│   │   ├── network/        # TCP/TLS/WebSocket
│   │   ├── plugin/         # 插件系统
│   │   ├── security/       # 加密引擎 + JWT
│   │   ├── social/         # 社交管理器
│   │   ├── storage/        # 本地存储
│   │   └── util/           # 工具函数
│   ├── devtools/cli/       # CLI REPL + 22 命令
│   ├── include/            # 公共头文件
│   └── security/           # Rust crypto + NASM asm
├── data/ui/                # Web UI (存档)
├── docs/                   # 文档
├── tests/                  # 测试脚本
└── installer/              # NSIS 安装脚本
```

## 文档

- [构建指南](docs/BUILD.md)
- [测试指南](docs/TESTING.md)
- [协议说明](docs/PROTOCOL.md)
- [安全审计报告](docs/SECURITY_AUDIT.md)
- [I2P 集成设计](docs/I2P_INTEGRATION.md)
- [ASM 混淆算法](docs/ASM_OBFUSCATION.md)

## 许可证

GPLv3 — 详见 [LICENSE](LICENSE)
