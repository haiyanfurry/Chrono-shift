#!/bin/bash
# build_i2pd.sh — 编译 i2pd 静态库供 chrono-client 嵌入使用
#
# 需要: MinGW-w64, Boost, OpenSSL, zlib
# 源码: C:/Users/haiyan/i2pd
# 输出: libi2pd.a, libi2pd_client.a → client/build/

set -e

I2PD_SRC="/c/Users/haiyan/i2pd"
CHRONO_DIR="/c/Users/haiyan/Chrono-shift/client"

echo "=== 编译 i2pd 静态库 ==="

cd "$I2PD_SRC"

# 来源 i2pd 的 Makefile.mingw 编译设置
CXX=g++
CXXFLAGS="-std=c++20 -fPIC -msse -DWIN32_LEAN_AND_MEAN -D_WIN32_WINNT=0x0601"
INCFLAGS="-I${I2PD_SRC} -I${I2PD_SRC}/Win32 -I${I2PD_SRC}/daemon"

# 收集 libi2pd 源文件 (核心路由器)
LIBI2PD_SRCS=$(ls "$I2PD_SRC"/libi2pd/*.cpp 2>/dev/null | grep -v 'test' || true)
# 收集 libi2pd_client 源文件 (SAM/SOCKS/BOB 协议)
LIBCLIENT_SRCS=$(ls "$I2PD_SRC"/libi2pd_client/*.cpp 2>/dev/null | grep -v 'test' || true)

echo "编译 libi2pd ($(echo "$LIBI2PD_SRCS" | wc -l) 源文件)..."
OBJ_DIR="$CHRONO_DIR/build/i2pd_obj"
mkdir -p "$OBJ_DIR"

for src in $LIBI2PD_SRCS; do
    obj="$OBJ_DIR/$(basename $src .cpp).o"
    $CXX $CXXFLAGS $INCFLAGS -c "$src" -o "$obj" &
done
wait

echo "创建 libi2pd.a..."
ar rcs "$CHRONO_DIR/build/libi2pd.a" "$OBJ_DIR"/*.o

echo "编译 libi2pd_client ($(echo "$LIBCLIENT_SRCS" | wc -l) 源文件)..."
for src in $LIBCLIENT_SRCS; do
    obj="$OBJ_DIR/client_$(basename $src .cpp).o"
    $CXX $CXXFLAGS $INCFLAGS -c "$src" -o "$obj" &
done
wait

echo "创建 libi2pd_client.a..."
ar rcs "$CHRONO_DIR/build/libi2pd_client.a" "$OBJ_DIR"/client_*.o

echo "=== i2pd 静态库编译完成 ==="
echo "  libi2pd.a         → $CHRONO_DIR/build/libi2pd.a"
echo "  libi2pd_client.a  → $CHRONO_DIR/build/libi2pd_client.a"
