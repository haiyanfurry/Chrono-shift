#!/bin/bash
# build_tor.sh — 编译 tor.exe 供 chrono-client 子进程嵌入
#
# Tor 源码: C:/Users/haiyan/tor
# 输出: tor.exe → chrono-client 可执行目录
#
# Tor 使用 autotools 构建系统。
# 在 MSYS2/MinGW 环境下运行此脚本。

set -e

TOR_SRC="/c/Users/haiyan/tor"
CHRONO_DIR="/c/Users/haiyan/Chrono-shift/client"

echo "=== 编译 Tor (子进程嵌入) ==="

cd "$TOR_SRC"

# 检查是否已配置
if [ ! -f "Makefile" ]; then
    echo "运行 autoreconf..."
    autoreconf -i 2>/dev/null || true

    echo "运行 configure..."
    ./configure \
        --disable-asciidoc \
        --disable-system-torrc \
        --disable-unittests \
        --disable-module-dirauth \
        --disable-module-relay \
        --enable-pic \
        --prefix="$CHRONO_DIR/build/tor_dist" \
        CC=gcc \
        CFLAGS="-O2 -fPIC"
fi

echo "编译 tor..."
make -j$(nproc) 2>&1 | tail -20

# 复制到 chrono-client 目录
mkdir -p "$CHRONO_DIR/build"
cp src/app/tor.exe "$CHRONO_DIR/build/tor.exe" 2>/dev/null || \
cp src/app/tor "$CHRONO_DIR/build/tor.exe" 2>/dev/null || true

echo "=== Tor 编译完成 ==="
echo "  tor.exe → $CHRONO_DIR/build/tor.exe"
echo ""
echo "如果编译失败 (autotools 在 Windows 上兼容性差):"
echo "  方案A: 下载 Tor Expert Bundle: https://www.torproject.org/download/tor/"
echo "  方案B: 使用 MSYS2 pacman -S tor"
echo "  方案C: 使用现有的 tor.exe (Tor Browser 目录下)"
