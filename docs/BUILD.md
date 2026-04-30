# Chrono-shift 构建指南

## 环境要求

- Windows 10/11 x64
- GCC 15.2.0+ (MinGW)
- Rust 1.95.0+ (stable-x86_64-pc-windows-gnu)
- CMake 3.15+
- NSIS v3.12
- WebView2 Runtime (Windows 10/11 内置)

## 构建步骤

### 1. 编译 Rust 安全模块

```bash
# 服务端
cd server/security
cargo build --release

# 客户端
cd client/security
cargo build --release
```

### 2. 编译 C 后端

```bash
# 服务端
cd server
cmake -B build -G "MinGW Makefiles"
cmake --build build

# 客户端
cd client
cmake -B build -G "MinGW Makefiles"
cmake --build build
```

### 3. 一键构建

```bash
make all
```

### 4. 制作 NSIS 安装包

```bash
make installer
```

## 运行

```bash
# 启动服务端
server/build/chrono-server.exe --port 8080

# 启动客户端
client/build/chrono-client.exe
```
