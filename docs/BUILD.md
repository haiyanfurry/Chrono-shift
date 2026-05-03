# Chrono-shift 构建指南

> **版本**: v2.0.0
> **变更**: 新增 NASM ASM 私有加密 + DevTools CLI + Rust 安全模块

## 概述

Chrono-shift 支持 **Linux** 和 **Windows** 两个平台。客户端使用 C/C++ + WebView2 (Windows) / WebKitGTK (Linux) 构建，
CLI 工具使用纯 C 编写，安全模块使用 Rust 编写，私有加密核心使用 NASM 汇编编写。

> **重要**: v0.2.0 起 **TLS (HTTPS) 为强制要求**，OpenSSL 是必需依赖。
> 不再支持纯 HTTP 明文模式。自签名证书可在首次启动时自动生成。

> **v0.3.0 变更**: 服务端 (`server/`) 已移除，项目聚焦于客户端和 CLI 工具。

> **v2.0.0 变更**: 新增 NASM 汇编私有加密 (ChronoStream v1)、DevTools CLI (30+ 命令)、
> Rust 安全模块全面集成、AI 多提供商支持、插件系统。

项目结构:
```
chrono-shift/
├── client/                      # 桌面客户端 + CLI 工具
│   ├── include/                 # 公共 C 头文件
│   ├── src/                     # C/C++ 源码
│   │   ├── network/             # 网络通信模块 (TCP/HTTP/WS/TLS)
│   │   ├── security/            # C++ 安全模块 (CryptoEngine)
│   │   ├── storage/             # 本地存储模块
│   │   ├── app/                 # 应用外壳 (WebView2 集成等)
│   │   ├── ai/                  # AI 多提供商 (OpenAI/Gemini/Custom)
│   │   ├── plugin/              # 插件系统
│   │   └── util/                # 工具函数
│   ├── ui/                      # 前端 HTML/CSS/JS
│   ├── devtools/                # 开发者工具
│   │   ├── cli/                 # 独立 CLI (C99, 30+ 命令)
│   │   └── core/                # In-App DevTools (C++ + JS)
│   ├── security/                # Rust 安全模块
│   │   ├── src/                 # Rust 源码
│   │   ├── asm/                 # NASM 汇编 (ChronoStream v1)
│   │   └── include/             # FFI C 头文件
│   ├── tools/                   # 附加工具 (debug_cli, stress_test)
│   └── plugins/                 # 插件目录
├── installer/                   # NSIS 安装脚本
└── docs/                        # 文档
```

---

## Linux 构建

### 依赖安装

**Ubuntu/Debian:**
```bash
sudo apt update
sudo apt install -y \
    build-essential \
    cmake          \
    pkg-config     \
    libssl-dev     \
    libwebkit2gtk-4.1-dev \
    nasm           \
    cargo          \
    rustc
```

**Arch Linux:**
```bash
sudo pacman -S base-devel cmake openssl webkit2gtk nasm rust
```

**Fedora/RHEL:**
```bash
sudo dnf groupinstall "Development Tools"
sudo dnf install cmake openssl-devel webkit2gtk4.1-devel nasm cargo
```

> **注意**: NASM 3.01+ 是 v2.0.0 新增的必需依赖，用于编译 ChronoStream v1 私有加密核心。

### 编译 Rust 安全模块 (含 ASM)

```bash
cd client/security
cargo build --release
```

此步骤会自动：
1. 调用 `build.rs` 编译 `asm/obfuscate.asm` → `asm/obfuscate.obj`
2. 编译所有 Rust 源码 (crypto.rs, asm_bridge.rs, sanitizer.rs, session.rs, secure_storage.rs)
3. 链接 ASM 目标文件到 Rust 库
4. 输出 `libchrono_client_security.a` (staticlib) + `chrono_client_security.dll` (cdylib)

**运行单元测试**:
```bash
cd client/security
cargo test --lib

# 预期: 4 个测试全部通过
# test test_obfuscate_deobfuscate_roundtrip ... ok
# test test_empty_data ... ok
# test test_single_obfuscate ... ok
# test test_different_keys ... ok
```

### 编译 DevTools CLI

```bash
cd client/devtools/cli
make
# 输出: chrono-devtools (Linux) 或 chrono-devtools.exe (Windows)
```

**使用根目录 Makefile:**
```bash
# 编译 CLI 工具 (包含 DevTools CLI + 旧版 debug_cli + stress_test)
make cli_tools
```

### 编译桌面客户端 (C++)

```bash
cd client
cmake -B build
cmake --build build
# 输出: client/build/chrono-client
```

### 生成自签名测试证书

```bash
mkdir -p client/certs

# 生成 RSA 2048 自签名证书，CN=127.0.0.1
openssl req -x509 -newkey rsa:2048 \
  -keyout client/certs/server.key -out client/certs/server.crt \
  -days 365 -nodes \
  -subj "/CN=127.0.0.1" \
  -addext "subjectAltName=IP:127.0.0.1"
```

> **证书文件位置**: `client/certs/server.crt`（证书）和 `client/certs/server.key`（私钥）

### 验证 TLS 连接

```bash
openssl s_client -connect 127.0.0.1:4443
```

**预期输出（TLS 握手成功）：**
```
New, TLSv1.3, Cipher is TLS_AES_128_GCM_SHA256
SSL-Session:
    Protocol  : TLSv1.3
    Cipher    : TLS_AES_128_GCM_SHA256
Verification: OK
```

---

## Windows 构建

### 依赖安装

1. **MinGW-w64 GCC** (推荐 13.0+) — 必须包含 OpenSSL
   - 推荐: https://winlibs.com/ (整合包已包含 OpenSSL)
   - 或通过 MSYS2: `pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-openssl`

2. **NASM** 3.01+ — **v2.0.0 新增必需依赖**
   - Chocolatey: `choco install nasm`
   - 或手动下载: https://www.nasm.us/pub/nasm/releasebuilds/3.01/win64/
   - 确保 `nasm.exe` 在 `PATH` 中

3. **CMake** 3.15+
   - 下载: https://cmake.org/download/

4. **Rust** 1.70+
   - 安装: https://rustup.rs/
   - 目标: `stable-x86_64-pc-windows-gnu` (MinGW)

5. **NSIS** v3.12+ (制作安装包)
   - 下载: https://nsis.sourceforge.io/Download

6. **WebView2 Runtime** (Windows 10/11 内置)

### 编译 Rust 安全模块 (含 ASM)

```bash
cd client/security
cargo build --release
```

> **注意**: NASM 必须在 PATH 中可用。如果 NASM 未安装或不在 PATH 中，`build.rs` 会报错：
> ```
> "NASM 未找到，请确保 nasm 在 PATH 中"
> ```

### 编译 DevTools CLI

```bash
cd client/devtools/cli

# 使用 Makefile (MinGW)
mingw32-make

# 或直接编译:
gcc -std=c99 -Wall -Wextra -I. main.c net_http.c commands/init_commands.c \
    commands/cmd_health.c commands/cmd_endpoint.c commands/cmd_token.c \
    commands/cmd_ipc.c commands/cmd_user.c commands/cmd_session.c \
    commands/cmd_config.c commands/cmd_storage.c commands/cmd_crypto.c \
    commands/cmd_network.c commands/cmd_ws.c commands/cmd_msg.c \
    commands/cmd_friend.c commands/cmd_db.c commands/cmd_connect.c \
    commands/cmd_disconnect.c commands/cmd_tls_info.c commands/cmd_gen_cert.c \
    commands/cmd_json.c commands/cmd_trace.c commands/cmd_obfuscate.c \
    commands/cmd_ping.c commands/cmd_watch.c commands/cmd_rate_test.c \
    -o chrono-devtools.exe -lws2_32 -lssl -lcrypto
```

### 编译桌面客户端 (C++)

```bash
cd client
cmake -B build -G "MinGW Makefiles"
cmake --build build
# 输出: client/build/Release/chrono-client.exe
```

### 生成自签名测试证书

```bash
cd client

# 创建证书目录（如不存在）
mkdir certs

# 生成 RSA 2048 自签名证书，CN=127.0.0.1
openssl req -x509 -newkey rsa:2048 ^
  -keyout certs/server.key -out certs/server.crt ^
  -days 365 -nodes ^
  -subj "/CN=127.0.0.1" ^
  -addext "subjectAltName=IP:127.0.0.1"
```

### 制作安装包

```bash
# 确保已编译所有组件
make installer
# 或手动:
makensis installer/client_installer.nsi
```

---

## 一键构建 (根项目)

根目录 Makefile 支持一键构建所有组件:

```bash
# 编译全部 (Rust + C + DevTools CLI)
make all

# 仅 DevTools CLI
make cli_tools

# 仅客户端
make client

# 制作安装包
make installer

# 清理
make clean
```

根目录 CMakeLists.txt 支持子目录构建:
```bash
cmake -B build
cmake --build build
```

---

## DevTools CLI 使用

### 交互模式

```bash
./client/devtools/cli/chrono-devtools
# 进入交互式命令行，输入 help 查看所有命令
```

### 单命令模式

```bash
# 基础
chrono-devtools health
chrono-devtools endpoint set --host 127.0.0.1 --port 4443

# 加密测试
chrono-devtools crypto test                       # AES-256-GCM 加密测试
chrono-devtools obfuscate genkey                  # 生成随机 512 位密钥
chrono-devtools obfuscate encrypt --data "Hello" --key <hex_key>
chrono-devtools obfuscate decrypt --data <b64> --key <hex_key>
chrono-devtools obfuscate test                    # 完整加密/解密/验证

# 网络诊断
chrono-devtools ping --host 127.0.0.1 --port 4443
chrono-devtools network test --host 127.0.0.1 --port 4443
chrono-devtools tls info

# WebSocket 调试
chrono-devtools ws connect --host 127.0.0.1 --port 4443
chrono-devtools ws send '{"type":"ping"}'
chrono-devtools ws monitor

# 会话管理
chrono-devtools session show
chrono-devtools session login --user test --pass ****

# 性能测试
chrono-devtools rate_test --threads 4 --qps 100 --duration 30
```

### 所有命令列表

| 命令 | 用途 |
|------|------|
| `health` | 服务健康检查 |
| `endpoint` | 服务器端点配置 |
| `token` | 登录令牌管理 |
| `ipc` | IPC 消息调试 |
| `user` | 用户注册/登录/资料 |
| `session` | 本地会话管理 |
| `config` | 本地配置管理 |
| `storage` | 安全存储查看 |
| `crypto` | E2E 加密测试 |
| `network` | 网络连接诊断 |
| `ws` | WebSocket 连接/发送/接收/监控 |
| `msg` | 消息数据库操作 |
| `friend` | 好友管理 |
| `db` | 数据库浏览 |
| `connect` | 连接到服务器 |
| `disconnect` | 断开服务器连接 |
| `tls` | TLS 连接信息 |
| `gen_cert` | 生成自签名证书 |
| `json` | JSON 解析/格式化 |
| `trace` | HTTP 请求追踪 |
| `obfuscate` | ASM 混淆加密/解密/密钥生成 |
| `ping` | 延迟测试 |
| `watch` | 实时连接监控 |
| `rate_test` | QPS 压力测试 |

---

## 旧版 CLI 工具

### debug_cli — 调试接口

```bash
# 交互模式
./client/tools/debug_cli

# 单命令模式
./client/tools/debug_cli health
./client/tools/debug_cli "session show"
./client/tools/debug_cli "config show"
./client/tools/debug_cli "crypto test"
./client/tools/debug_cli "network test 127.0.0.1 4443"
```

支持以下本地命令:
- `session show/login/logout` — 会话管理
- `config show/set` — 配置管理
- `storage list/get` — 安全存储
- `crypto test` — AES-256-GCM 加密测试
- `network test <host> <port>` — 网络诊断
- `ipc types/send/capture` — IPC 消息
- `ws connect/send/recv/close/status/monitor` — WebSocket

### stress_test — 压力测试

```bash
./client/tools/stress_test --host 127.0.0.1 --port 4443 --threads 4 --qps 100 --duration 30
```

---

## 构建配置

### CMake 选项

| 选项 | 说明 | 默认值 |
|------|------|--------|
| `CMAKE_BUILD_TYPE` | Debug / Release | (未设置) |
| `CMAKE_C_FLAGS` | 附加 C 编译标志 | — |

示例 (Debug 构建):
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

### Rust 特性 (安全模块)

Rust 安全模块支持以下 Cargo features (在 `client/security/Cargo.toml` 中配置):

- `default` — 基础加密、会话管理
- `full` — 全部功能 (默认)

### 构建产物清单

| 组件 | 输出路径 | 类型 |
|------|---------|------|
| 桌面客户端 | `client/build/Release/chrono-client.exe` | C++ 可执行文件 |
| DevTools CLI | `client/devtools/cli/chrono-devtools.exe` | C 可执行文件 |
| Rust 静态库 | `client/security/target/release/libchrono_client_security.a` | Rust staticlib |
| Rust 动态库 | `client/security/target/release/chrono_client_security.dll` | Rust cdylib |
| NASM 目标文件 | `client/security/asm/obfuscate.obj` | COFF 目标文件 |
| 安装包 | `installer/Chrono-shift-Setup.exe` | NSIS 安装包 |

---

## 常见问题

### Q: NASM 未找到？
A: v2.0.0 新增 NASM 作为必需依赖。请安装 NASM 3.01+ 并确保在 PATH 中：
- Windows: `choco install nasm` 或从 https://www.nasm.us/ 下载
- Linux: `sudo apt install nasm` (Ubuntu) / `sudo pacman -S nasm` (Arch)

### Q: Linux 编译时报 `openssl/ssl.h` 未找到？
A: OpenSSL 开发库未安装。v0.2.0 起 TLS 为强制要求，必须安装 OpenSSL：
- Ubuntu/Debian: `sudo apt install libssl-dev`
- Arch: `sudo pacman -S openssl`
- Fedora/RHEL: `sudo dnf install openssl-devel`

### Q: Windows 上使用官方 OpenSSL 安装程序后编译仍失败？
A: 官方 OpenSSL for Windows 提供的是 VC (Visual C++) 格式的库文件，MinGW GCC 无法直接链接。解决方案：
1. 使用 winlibs.com 提供的 MinGW 整合包（已包含 OpenSSL）
2. 或通过 MSYS2 安装: `pacman -S mingw-w64-x86_64-openssl`
3. 然后确保编译命令中的 `-I` 和 `-L` 路径指向正确的安装位置

### Q: 如何在没有 NASM 的情况下编译？
A: 如果不需要 ASM 混淆功能，可以跳过 Rust 安全模块编译：
```bash
# 仅编译 C++ 客户端 (跳过 Rust 安全模块)
cd client
cmake -B build -G "MinGW Makefiles"
cmake --build build
# 注意: ASM 混淆功能将不可用
```

### Q: 如何在没有 OpenSSL 的情况下编译？
A: v0.2.0 起 TLS (OpenSSL) 为强制依赖，不再支持无加密的纯 HTTP 模式。
请在 Linux 上安装 `libssl-dev` 或在 Windows 上安装 MinGW 兼容的 OpenSSL。

### Q: 旧的 server/ 目录哪去了？
A: 自 v0.3.0 起，服务端 (`server/`) 已被移除，项目专注于客户端和 CLI 工具。
历史服务端代码可参考 [`plans/ARCHITECTURE.md`](../plans/ARCHITECTURE.md) 中的架构描述。

### Q: 如何交叉编译？
A: 当前不支持交叉编译。请直接在目标平台上构建。

### Q: Rust 安全模块编译很慢？
A: 首次编译 Rust 模块需要下载依赖 (aes-gcm, base64, ring 等)，之后增量编译很快。
使用 `--release` 标志可生成优化后的二进制。如果不需要 E2E/ASM 功能，可跳过此步骤。
