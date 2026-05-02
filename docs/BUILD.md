# Chrono-shift 构建指南

## 概述

Chrono-shift 支持 **Linux** 和 **Windows** 两个平台。客户端使用 C/C++ + WebView2 (Windows) / WebKitGTK (Linux) 构建，
CLI 工具使用纯 C 编写，安全模块使用 Rust 编写。

> **重要**: v0.2.0 起 **TLS (HTTPS) 为强制要求**，OpenSSL 是必需依赖。
> 不再支持纯 HTTP 明文模式。自签名证书可在首次启动时自动生成。

> **v0.3.0 变更**: 服务端 (`server/`) 已移除，项目聚焦于客户端和 CLI 工具。
> 历史服务端代码参考 [`plans/ARCHITECTURE.md`](plans/ARCHITECTURE.md)。

项目结构:
```
chrono-shift/
├── client/                  # 桌面客户端 + CLI 工具
│   ├── include/             # 头文件
│   ├── src/                 # C/C++ 源码
│   │   ├── network/         # 网络通信模块 (TCP/HTTP/WS/TLS)
│   │   ├── security/        # 安全模块 (加密/令牌)
│   │   ├── storage/         # 本地存储模块
│   │   ├── app/             # 应用外壳 (WebView2 集成等)
│   │   └── util/            # 工具函数
│   ├── ui/                  # 前端 HTML/CSS/JS
│   ├── tools/               # CLI 工具 (debug_cli, stress_test)
│   ├── security/            # Rust 安全模块
│   └── CMakeLists.txt
├── installer/               # NSIS 安装脚本
└── docs/                    # 文档
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
    cargo          \
    rustc
```

**Arch Linux:**
```bash
sudo pacman -S base-devel cmake openssl webkit2gtk rust
```

**Fedora/RHEL:**
```bash
sudo dnf groupinstall "Development Tools"
sudo dnf install cmake openssl-devel webkit2gtk4.1-devel cargo
```

### 编译 Rust 安全模块 (可选)

```bash
cd client/security
cargo build --release
```

### 编译 CLI 工具

**使用根目录 Makefile:**
```bash
# 编译 CLI 调试工具 + 压力测试工具
make cli_tools

# 输出: client/tools/debug_cli 和 client/tools/stress_test
```

**使用 client/tools/Makefile:**
```bash
cd client/tools

# Linux
make -f Makefile

# 或直接编译:
gcc -std=c99 -Wall -Wextra -I../include \
    debug_cli.c -o debug_cli -lssl -lcrypto -lm

gcc -std=c99 -Wall -I../include \
    stress_test.c -o stress_test -lm
```

> **提示**: CLI 工具依赖 OpenSSL (`-lssl -lcrypto`)，需提前安装。

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

2. **CMake** 3.15+
   - 下载: https://cmake.org/download/

3. **Rust** 1.70+
   - 安装: https://rustup.rs/
   - 目标: `stable-x86_64-pc-windows-gnu` (MinGW)

4. **NSIS** v3.12+ (制作安装包)
   - 下载: https://nsis.sourceforge.io/Download

5. **WebView2 Runtime** (Windows 10/11 内置)

### 编译 Rust 安全模块 (可选)

```bash
cd client/security
cargo build --release
```

### 编译 CLI 工具

```bash
cd client/tools

# 使用 Makefile (MinGW)
mingw32-make -f Makefile

# 或直接编译:
gcc -std=c99 -Wall -I../include \
    debug_cli.c -o debug_cli.exe -lws2_32 -lssl -lcrypto

gcc -std=c99 -Wall -I../include \
    stress_test.c -o stress_test.exe -lws2_32 -lm
```

> **注意**: v0.2.0 起 TLS 为强制要求，必须安装 MinGW 兼容的 OpenSSL。
> 推荐使用 winlibs.com 整合包（已内置 OpenSSL）或通过 MSYS2 安装。
> 如果 OpenSSL 安装在非标准路径，需调整 `-I` 和 `-L` 参数。

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
# 编译全部 (Rust + C)
make all

# 仅 CLI 工具
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

## CLI 工具使用

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

---

## 常见问题

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

### Q: 如何在没有 OpenSSL 的情况下编译？
A: v0.2.0 起 TLS (OpenSSL) 为强制依赖，不再支持无加密的纯 HTTP 模式。
请在 Linux 上安装 `libssl-dev` 或在 Windows 上安装 MinGW 兼容的 OpenSSL。

### Q: 旧的 server/ 目录哪去了？
A: 自 v0.3.0 起，服务端 (`server/`) 已被移除，项目专注于客户端和 CLI 工具。
历史服务端代码可参考 [`plans/ARCHITECTURE.md`](plans/ARCHITECTURE.md) 中的架构描述。

### Q: 如何交叉编译？
A: 当前不支持交叉编译。请直接在目标平台上构建。
