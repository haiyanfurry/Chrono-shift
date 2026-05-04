#!/bin/bash
# download_binaries.sh — 下载 Tor 和 i2pd 预编译二进制文件
#
# Tor Expert Bundle: https://www.torproject.org/download/tor/
# i2pd Windows: https://github.com/PurpleI2P/i2pd/releases
#
# 输出: client/build/tor/tor.exe, client/build/i2pd/i2pd.exe

set -e

CHRONO_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BIN_DIR="$CHRONO_DIR/client/build"

echo "=== 下载传输层二进制文件 ==="

# --- Tor Expert Bundle ---
TOR_URL="https://archive.torproject.org/tor-package-archive/torbrowser/14.5.4/tor-expert-bundle-windows-x86_64-14.5.4.tar.gz"
TOR_DIR="$BIN_DIR/tor"

if [ ! -f "$TOR_DIR/tor.exe" ]; then
    echo "下载 Tor Expert Bundle..."
    mkdir -p "$TOR_DIR"
    TOR_ARCHIVE="$BIN_DIR/tor-bundle.tar.gz"

    if command -v wget &>/dev/null; then
        wget -O "$TOR_ARCHIVE" "$TOR_URL" 2>/dev/null || {
            echo "  Tor 下载失败 (需要代理)"
            echo "  手动下载: https://www.torproject.org/download/tor/"
            echo "  解压 tor.exe 到 $TOR_DIR/"
        }
    elif command -v curl &>/dev/null; then
        curl -L -o "$TOR_ARCHIVE" "$TOR_URL" 2>/dev/null || true
    fi

    if [ -f "$TOR_ARCHIVE" ] && [ -s "$TOR_ARCHIVE" ]; then
        tar xzf "$TOR_ARCHIVE" -C "$TOR_DIR" --strip-components=1 2>/dev/null || true
        rm -f "$TOR_ARCHIVE"
    fi
else
    echo "  Tor: 已存在 ($TOR_DIR/tor.exe)"
fi

# --- i2pd Windows ---
# 最新 release: https://github.com/PurpleI2P/i2pd/releases/latest
I2PD_URL="https://github.com/PurpleI2P/i2pd/releases/download/2.56.0/i2pd_2.56.0_win64_mingw.zip"
I2PD_DIR="$BIN_DIR/i2pd"

if [ ! -f "$I2PD_DIR/i2pd.exe" ]; then
    echo "下载 i2pd Windows 二进制..."
    mkdir -p "$I2PD_DIR"
    I2PD_ARCHIVE="$BIN_DIR/i2pd.zip"

    if command -v wget &>/dev/null; then
        wget -O "$I2PD_ARCHIVE" "$I2PD_URL" 2>/dev/null || {
            echo "  i2pd 下载失败"
            echo "  手动下载: https://github.com/PurpleI2P/i2pd/releases"
            echo "  解压 i2pd.exe 到 $I2PD_DIR/"
        }
    elif command -v curl &>/dev/null; then
        curl -L -o "$I2PD_ARCHIVE" "$I2PD_URL" 2>/dev/null || true
    fi

    if [ -f "$I2PD_ARCHIVE" ] && [ -s "$I2PD_ARCHIVE" ]; then
        unzip -o "$I2PD_ARCHIVE" -d "$I2PD_DIR" 2>/dev/null || true
        rm -f "$I2PD_ARCHIVE"
    fi
else
    echo "  i2pd: 已存在 ($I2PD_DIR/i2pd.exe)"
fi

echo ""
echo "=== 二进制文件状态 ==="
[ -f "$TOR_DIR/tor.exe" ] && echo "  [OK] Tor: $TOR_DIR/tor.exe" || echo "  [--] Tor: 待下载"
[ -f "$I2PD_DIR/i2pd.exe" ] && echo "  [OK] i2pd: $I2PD_DIR/i2pd.exe" || echo "  [--] i2pd: 待下载"
echo ""
echo "如果自动下载失败，请手动下载并放置到上述路径。"
