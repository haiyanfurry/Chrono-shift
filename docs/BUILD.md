# Chrono-shift 构建指南

## 概述

Chrono-shift 支持 **Linux** 和 **Windows** 两个平台。服务器端代码完全兼容 POSIX，
使用 epoll (Linux) 或 WSAPoll (Windows) 事件驱动模型。

> **重要**: v0.2.0 起 **TLS (HTTPS) 为强制要求**，OpenSSL 是必需依赖。
> 不再支持纯 HTTP 明文模式。自签名证书可在首次启动时自动生成。

项目结构:
```
chrono-shift/
├── server/          # 独立服务器项目 (跨平台)
│   ├── include/     # 头文件
│   ├── src/         # C 源码
│   ├── security/    # Rust 安全模块
│   ├── tools/       # 调试与测试工具
│   ├── certs/       # TLS 证书 (首次启动自动生成)
│   ├── CMakeLists.txt
│   └── Makefile
├── client/          # Windows 客户端 (WebView2)
│   ├── include/
│   ├── src/
│   ├── ui/          # 前端 HTML/CSS/JS
│   ├── security/    # Rust 安全模块
│   └── CMakeLists.txt
├── installer/       # NSIS 安装脚本
└── docs/            # 文档
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
    cargo          \
    rustc
```

**Arch Linux:**
```bash
sudo pacman -S base-devel cmake openssl rust
```

**Fedora/RHEL:**
```bash
sudo dnf groupinstall "Development Tools"
sudo dnf install cmake openssl-devel cargo
```

### 编译 Rust 安全模块 (可选)

```bash
cd server/security
cargo build --release
```

### 编译 C 服务器

**使用 CMake (推荐):**
```bash
cd server
cmake -B build
cmake --build build
# 输出: server/build/chrono-server
```

**使用 Makefile:**
```bash
cd server

# 编译 Rust 模块 (可选)
make rust

# 编译 C 代码 (TLS 强制启用，需安装 libssl-dev)
make build

# 编译调试工具
make debug-cli

# 编译压力测试工具
make stress-test

# 清理
make clean
# 输出: server/out/chrono-server
```

### 生成自签名测试证书

```bash
cd server

# 创建证书目录（如不存在）
mkdir -p certs

# 生成 RSA 2048 自签名证书，CN=127.0.0.1
openssl req -x509 -newkey rsa:2048 \
  -keyout certs/server.key -out certs/server.crt \
  -days 365 -nodes \
  -subj "/CN=127.0.0.1" \
  -addext "subjectAltName=IP:127.0.0.1"
```

> **证书文件位置**: `server/certs/server.crt`（证书）和 `server/certs/server.key`（私钥）
>
> **提示**: 如果不指定证书，服务器启动时会自动在 `./certs/` 目录生成自签名证书。

### 运行服务器

```bash
# 方式一: 自动生成证书 (推荐用于测试)
./server/out/chrono-server --port 4443

# 方式二: 使用已有证书
./server/out/chrono-server --port 4443 \
    --tls-cert ./certs/server.crt --tls-key ./certs/server.key
```

**命令行选项:**
| 选项 | 说明 | 默认值 |
|------|------|--------|
| `--port <port>` | 监听端口 | `4443` |
| `--db <path>` | 数据库路径 | `./data/db/chrono.db` |
| `--storage <path>` | 文件存储路径 | `./data/storage` |
| `--log-level <0-3>` | 日志级别 (0=DEBUG, 3=ERROR) | `1` |
| `--tls-cert <path>` | TLS 证书文件路径 (PEM) | 自动生成 |
| `--tls-key <path>` | TLS 私钥文件路径 (PEM) | 自动生成 |
| `--help` | 显示帮助 | — |

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
cd server/security
cargo build --release

cd client/security
cargo build --release
```

### 编译 C 服务器

> **注意**: v0.2.0 起 TLS 为强制要求，必须安装 MinGW 兼容的 OpenSSL。
> 推荐使用 winlibs.com 整合包（已内置 OpenSSL）或通过 MSYS2 安装。

**方式 A：使用 Makefile（推荐）**

```bash
cd server

# 编译全部 (TLS 强制启用)
mingw32-make build

# 编译调试工具
mingw32-make debug-cli

# 清理编译产物
mingw32-make clean

# 输出: server/out/chrono-server.exe
```

> 如果 OpenSSL 安装在非标准路径，需要修改 Makefile 中的 `-ID:/mys32/mingw64/include` 和 `-LD:/mys32/mingw64/lib`。

**方式 B：使用 CMake (MinGW)**
```bash
cd server
cmake -B build -G "MinGW Makefiles"
cmake --build build
# 输出: server/build/chrono-server.exe
```

### 生成自签名测试证书

```bash
cd server

# 创建证书目录（如不存在）
mkdir certs

# 生成 RSA 2048 自签名证书，CN=127.0.0.1
openssl req -x509 -newkey rsa:2048 ^
  -keyout certs/server.key -out certs/server.crt ^
  -days 365 -nodes ^
  -subj "/CN=127.0.0.1" ^
  -addext "subjectAltName=IP:127.0.0.1"
```

### 运行服务器

```cmd
cd server

:: 将 OpenSSL DLL 目录加入 PATH (根据实际安装路径调整)
set PATH=C:\mingw64\bin;%PATH%

:: 启动 HTTPS 服务器
out\chrono-server.exe --port 4443

:: 或指定证书
out\chrono-server.exe --port 4443 --tls-cert certs\server.crt --tls-key certs\server.key
```

### 编译 C 客户端

```bash
cd client
cmake -B build -G "MinGW Makefiles"
cmake --build build
# 输出: client/build/Release/chrono-client.exe
```

### 制作安装包

```bash
# 确保已编译所有组件
make installer
# 或手动:
makensis installer/client_installer.nsi
makensis installer/server_installer.nsi
```

### 运行

```bash
# 启动服务端
server\out\chrono-server.exe --port 4443

# 启动客户端
client\build\Release\chrono-client.exe
```

---

## 一键构建 (根项目)

根目录 Makefile 支持一键构建所有组件:

```bash
# 编译全部 (Rust + C)
make all

# 仅服务端
make server

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

Rust 安全模块支持以下 Cargo features (在 `server/security/Cargo.toml` 中配置):

- `default` — 基础加密、JWT、密码哈希
- `full` — 全部功能 (默认)

---

## 常见问题

### Q: Linux 编译时报 `strcasecmp` 未定义？
A: 请确保已包含 `<strings.h>`。[`platform_compat.h`](../server/include/platform_compat.h) 已自动处理此问题。

### Q: Windows 编译时报 `epoll` 相关错误？
A: 这是正常的 —— epoll 代码在 `#ifdef PLATFORM_LINUX` 块内。
Windows 上使用 WSAPoll() 替代。在 MinGW 下编译不会有问题。

### Q: Rust 安全库未找到？
A: 服务器仍然可以编译和运行，只是不使用 Rust 加密模块
(仅使用内置的基础加密)。要启用完整安全功能，请先执行
`cd server/security && cargo build --release`。

### Q: Linux 编译时报 `openssl/ssl.h` 未找到？
A: OpenSSL 开发库未安装。v0.2.0 起 TLS 为强制要求，必须安装 OpenSSL：
- Ubuntu/Debian: `sudo apt install libssl-dev`
- Arch: `sudo pacman -S openssl`
- Fedora/RHEL: `sudo dnf install openssl-devel`

### Q: Windows 上使用官方 OpenSSL 安装程序后编译仍失败？
A: 官方 OpenSSL for Windows 提供的是 VC (Visual C++) 格式的库文件，MinGW GCC 无法直接链接。解决方案：
1. 使用 winlibs.com 提供的 MinGW 整合包（已包含 OpenSSL）
2. 或通过 MSYS2 安装: `pacman -S mingw-w64-x86_64-openssl`
3. 然后确保 Makefile 中的 `-ID:/mys32/mingw64/include` 和 `-LD:/mys32/mingw64/lib` 路径指向正确的安装位置

### Q: Windows 上 TLS 握手失败（SSL_accept 返回 -1）？
A: 这是 Windows 平台特有行为 — 非阻塞监听 socket 创建的子 socket 继承非阻塞模式，
而 OpenSSL 的 `SSL_accept()` 需要在阻塞模式下工作。解决方案：
1. 确保 `tls_server_wrap()` 在调用 `SSL_accept()` 前调用 `set_blocking(fd)`
2. 该修复已在 [`server/src/tls_server.c`](../server/src/tls_server.c) 中实现，
   详见 [`server/include/platform_compat.h`](../server/include/platform_compat.h) 中的 `set_blocking()` 函数

### Q: 运行 TLS 服务器时报 "无法加载 DLL"？ (Windows)
A: 这是因为 OpenSSL DLL (`libssl-3-x64.dll`, `libcrypto-3-x64.dll`) 不在 PATH 中。
解决方案：
```cmd
:: 将 MinGW 的 bin 目录加入 PATH (根据实际安装路径调整)
set PATH=C:\mingw64\bin;%PATH%
:: 然后重新运行服务器
out\chrono-server.exe --port 4443
```

### Q: 如何交叉编译？
A: 当前不支持交叉编译。请直接在目标平台上构建。

### Q: 如何在没有 OpenSSL 的情况下编译？
A: v0.2.0 起 TLS (OpenSSL) 为强制依赖，不再支持无加密的纯 HTTP 模式。
请在 Linux 上安装 `libssl-dev` 或在 Windows 上安装 MinGW 兼容的 OpenSSL。
