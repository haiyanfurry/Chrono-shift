#!/usr/bin/env bash
# ============================================================
# 墨竹 (Chrono-shift) 清理脚本 (Linux/macOS)
#
# 用途: 清理测试临时文件、编译中间产物、残留数据
# 用法: ./cleanup.sh [--all] [--test] [--build] [--logs] [--help]
# ============================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

CLEAN_ALL=0
CLEAN_TEST=0
CLEAN_BUILD=0
CLEAN_LOGS=0

# --- 颜色 ---
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}"
echo "============================================"
echo "   墨竹 清理脚本"
echo "============================================"
echo -e "${NC}"

# --- 解析参数 ---
while [ $# -gt 0 ]; do
    case "$1" in
        --all)   CLEAN_ALL=1 ;;
        --test)  CLEAN_TEST=1 ;;
        --build) CLEAN_BUILD=1 ;;
        --logs)  CLEAN_LOGS=1 ;;
        --help)
            echo "用法: $0 [选项]"
            echo ""
            echo "选项:"
            echo "  --all     清理所有内容（默认）"
            echo "  --test    仅清理测试临时文件"
            echo "  --build   仅清理编译中间文件"
            echo "  --logs    仅清理日志和报告"
            echo "  --help    显示此帮助"
            echo ""
            echo "示例:"
            echo "  $0 --all"
            echo "  $0 --build --logs"
            echo "  $0 --test"
            echo ""
            exit 0
            ;;
        *) echo "未知选项: $1"; exit 1 ;;
    esac
    shift
done

# 如果没有指定参数，默认清理所有
if [ "$CLEAN_ALL" -eq 0 ] && [ "$CLEAN_TEST" -eq 0 ] && [ "$CLEAN_BUILD" -eq 0 ] && [ "$CLEAN_LOGS" -eq 0 ]; then
    CLEAN_ALL=1
fi

echo -e "${YELLOW}清理配置:${NC}"
echo "  清理测试文件:  $([ "$CLEAN_TEST" -eq 1 ] || [ "$CLEAN_ALL" -eq 1 ] && echo "是" || echo "否")"
echo "  清理构建产物:  $([ "$CLEAN_BUILD" -eq 1 ] || [ "$CLEAN_ALL" -eq 1 ] && echo "是" || echo "否")"
echo "  清理日志文件:  $([ "$CLEAN_LOGS" -eq 1 ] || [ "$CLEAN_ALL" -eq 1 ] && echo "是" || echo "否")"
echo ""

# ========================================
# 1. 清理测试文件
# ========================================
if [ "$CLEAN_ALL" -eq 1 ] || [ "$CLEAN_TEST" -eq 1 ]; then
    echo -e "${BLUE}[1/3]${NC} 清理测试临时文件..."
    
    # 测试临时数据
    find tests/ -name "*.tmp" -delete 2>/dev/null && echo -e "  ${GREEN}[OK]${NC} 已删除 tests 目录临时文件"
    
    # 测试用户数据
    find data/ -name "test_*.json" -delete 2>/dev/null && echo -e "  ${GREEN}[OK]${NC} 已删除测试用户数据"
    
    # 回环测试日志
    rm -f loopback_server.log && echo -e "  ${GREEN}[OK]${NC} 已删除回环测试日志"
    
    # curl 临时文件
    rm -f /tmp/loopback_*.json 2>/dev/null || true
    
    echo -e "  ${GREEN}[OK]${NC} 测试文件清理完成"
    echo ""
fi

# ========================================
# 2. 清理构建产物
# ========================================
if [ "$CLEAN_ALL" -eq 1 ] || [ "$CLEAN_BUILD" -eq 1 ]; then
    echo -e "${BLUE}[2/3]${NC} 清理编译中间文件..."
    
    # 客户端构建
    if [ -d "client/build" ]; then
        rm -rf client/build
        echo -e "  ${GREEN}[OK]${NC} 已删除 client/build 目录"
    fi
    
    # 服务端构建
    if [ -d "server/build" ]; then
        rm -rf server/build
        echo -e "  ${GREEN}[OK]${NC} 已删除 server/build 目录"
    fi
    
    # CMake 生成文件
    if [ -d "build" ]; then
        rm -rf build
        echo -e "  ${GREEN}[OK]${NC} 已删除 build 目录"
    fi
    
    # 对象文件
    find . -name "*.o" -type f -delete 2>/dev/null && echo -e "  ${GREEN}[OK]${NC} 已删除 .o 文件"
    
    # 可执行文件（测试工具）
    rm -f stress_test stress_test.exe 2>/dev/null || true
    
    # Rust 构建产物
    if [ -d "server/security/target" ]; then
        rm -rf server/security/target
        echo -e "  ${GREEN}[OK]${NC} 已删除 server/security/target (Rust)"
    fi
    if [ -d "client/security/target" ]; then
        rm -rf client/security/target
        echo -e "  ${GREEN}[OK]${NC} 已删除 client/security/target (Rust)"
    fi
    
    # CMakeFiles 目录
    find . -name "CMakeFiles" -type d -exec rm -rf {} + 2>/dev/null || true
    find . -name "CMakeCache.txt" -delete 2>/dev/null || true
    find . -name "cmake_install.cmake" -delete 2>/dev/null || true
    find . -name "Makefile" -path "*/CMakeFiles/*" -delete 2>/dev/null || true
    
    echo -e "  ${GREEN}[OK]${NC} 构建产物清理完成"
    echo ""
fi

# ========================================
# 3. 清理日志和报告
# ========================================
if [ "$CLEAN_ALL" -eq 1 ] || [ "$CLEAN_LOGS" -eq 1 ]; then
    echo -e "${BLUE}[3/3]${NC} 清理日志和报告文件..."
    
    # 测试报告
    if [ -d "reports" ]; then
        rm -rf reports
        echo -e "  ${GREEN}[OK]${NC} 已删除 reports 目录"
    fi
    
    # 日志文件
    find . -name "*.log" -type f -delete 2>/dev/null && echo -e "  ${GREEN}[OK]${NC} 已删除 .log 文件"
    
    echo -e "  ${GREEN}[OK]${NC} 日志清理完成"
    echo ""
fi

echo -e "${GREEN}"
echo "============================================"
echo "   清理完成"
echo "   工作目录: $SCRIPT_DIR"
echo "============================================"
echo -e "${NC}"
