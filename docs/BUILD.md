# Chrono-shift 构建指南 v3.0.0

## 依赖

| 组件 | 版本要求 | 用途 |
|------|---------|------|
| GCC | 13+ (或 Clang 16+ / MSVC 2022 17.6+) | C++23 编译 |
| CMake | 3.20+ | 构建系统 |
| OpenSSL | 3.x | TLS 加密 |
| Rust | 1.70+ | 安全模块编译 |
| NASM | 2.16+ | ASM 混淆加密 |
| MinGW-w64 | 最新 | Windows 构建环境 (MSYS2) |

## Windows 构建 (MSYS2/MinGW)

```bash
# 安装依赖
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake
pacman -S mingw-w64-x86_64-openssl mingw-w64-x86_64-nasm
pacman -S mingw-w64-x86_64-rust

# 编译 Rust 安全库
cd client/security
cargo build --release

# 编译 NASM 汇编
cd client/security/asm
nasm -f win64 obfuscate.asm -o obfuscate.obj

# 编译 C++ 客户端
cd client
cmake -B build -G "MinGW Makefiles"
cmake --build build -j$(nproc)

# 运行
./build/chrono-client.exe
```

## 构建产物

```
client/build/chrono-client.exe   # CLI 主程序 (~5MB)
```

## CMake 选项

- `HTTPS_SUPPORT`: 通过 `find_package(OpenSSL)` 自动检测
- `RUST_FEATURE_ENABLED`: 检测到 `libchrono_client_security.a` 时自动启用
- 子系统: `CONSOLE` (纯 CLI, 无 GUI)

## 故障排除

### OpenSSL 未找到
```bash
# MSYS2
pacman -S mingw-w64-x86_64-openssl
# 或手动指定路径
cmake .. -DCMAKE_PREFIX_PATH=/path/to/openssl
```

### C++23 编译错误
```bash
# 确认 GCC 版本
gcc --version  # 需要 >= 13
# MSYS2 更新
pacman -Syu
```

### Rust 库未链接
```bash
cd client/security && cargo build --release
# 确认 libchrono_client_security.a 存在
ls target/release/libchrono_client_security.a
```
