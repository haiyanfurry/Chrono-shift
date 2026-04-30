# Chrono-shift 构建指南

## 概述

Chrono-shift 支持 **Linux** 和 **Windows** 两个平台。服务器端代码完全兼容 POSIX，
使用 epoll (Linux) 或 select (Windows) 事件驱动模型。

项目结构:
```
chrono-shift/
├── server/          # 独立服务器项目 (跨平台)
│   ├── include/     # 头文件
│   ├── src/         # C 源码
│   ├── security/    # Rust 安全模块
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
    libcurl4-openssl-dev \
    cargo          \
    rustc
```

**Arch Linux:**
```bash
sudo pacman -S base-devel cmake openssl curl rust
```

**Fedora/RHEL:**
```bash
sudo dnf groupinstall "Development Tools"
sudo dnf install cmake openssl-devel libcurl-devel cargo
```

### 编译 Rust 安全模块

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
make rust   # 编译 Rust 模块
make build  # 编译 C 代码
# 输出: server/build/chrono-server
```

### 运行服务器

```bash
./server/build/chrono-server --port 8080 --db ./data/db --log-level 1
```

**命令行选项:**
| 选项 | 说明 | 默认值 |
|------|------|--------|
| `--port <port>` | 监听端口 | `8080` |
| `--db <path>` | 数据库路径 | `./data/db/chrono.db` |
| `--storage <path>` | 文件存储路径 | `./data/storage` |
| `--log-level <0-3>` | 日志级别 (0=DEBUG, 3=ERROR) | `1` |
| `--help` | 显示帮助 | — |

---

## Windows 构建

### 依赖安装

1. **MinGW-w64 GCC** (推荐 13.0+)
   - 下载: https://winlibs.com/
   - 或通过 MSYS2: `pacman -S mingw-w64-x86_64-gcc`

2. **CMake** 3.15+
   - 下载: https://cmake.org/download/

3. **Rust** 1.70+
   - 安装: https://rustup.rs/
   - 目标: `stable-x86_64-pc-windows-gnu` (MinGW)

4. **NSIS** v3.12+ (制作安装包)
   - 下载: https://nsis.sourceforge.io/Download

5. **WebView2 Runtime** (Windows 10/11 内置)

### 编译 Rust 安全模块

```bash
cd server/security
cargo build --release

cd client/security
cargo build --release
```

### 编译 C 服务器

**使用 CMake (MinGW):**
```bash
cd server
cmake -B build -G "MinGW Makefiles"
cmake --build build
# 输出: server/build/chrono-server.exe
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
server\build\chrono-server.exe --port 8080

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
A: 请确保已包含 `<strings.h>`。platform_compat.h 已自动处理此问题。

### Q: Windows 编译时报 `epoll` 相关错误？
A: 这是正常的 —— epoll 代码在 `#ifdef PLATFORM_LINUX` 块内。
Windows 上使用 select() 替代。在 MinGW 下编译不会有问题。

### Q: Rust 安全库未找到？
A: 服务器仍然可以编译和运行，只是不使用 Rust 加密模块
(仅使用内置的基础加密)。要启用完整安全功能，请先执行
`cd server/security && cargo build --release`。

### Q: 如何交叉编译？
A: 当前不支持交叉编译。请直接在目标平台上构建。
