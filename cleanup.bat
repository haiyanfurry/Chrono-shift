@echo off
REM ============================================================
REM 墨竹 (Chrono-shift) 清理脚本 (Windows)
REM
REM 用途: 清理测试临时文件、编译中间产物、残留数据
REM 用法: cleanup.bat [--all] [--test] [--build] [--logs] [--help]
REM ============================================================
setlocal enabledelayedexpansion

set "SCRIPT_DIR=%~dp0"
cd /d "%SCRIPT_DIR%"

set CLEAN_ALL=0
set CLEAN_TEST=0
set CLEAN_BUILD=0
set CLEAN_LOGS=0

echo.
echo ============================================
echo   墨竹 清理脚本 (Windows)
echo ============================================
echo.

REM --- 解析参数 ---
:parse_args
if "%~1"=="" goto :done_parse
if /i "%~1"=="--all" set CLEAN_ALL=1
if /i "%~1"=="--test" set CLEAN_TEST=1
if /i "%~1"=="--build" set CLEAN_BUILD=1
if /i "%~1"=="--logs" set CLEAN_LOGS=1
if /i "%~1"=="--help" goto :print_help
shift
goto :parse_args
:done_parse

REM 如果没有指定参数，默认清理所有
if %CLEAN_ALL%==0 if %CLEAN_TEST%==0 if %CLEAN_BUILD%==0 if %CLEAN_LOGS%==0 set CLEAN_ALL=1

echo 清理配置:
echo   清理测试文件:  %CLEAN_TEST% / %CLEAN_ALL%
echo   清理构建产物:  %CLEAN_BUILD% / %CLEAN_ALL%
echo   清理日志文件:  %CLEAN_LOGS% / %CLEAN_ALL%
echo.

REM ========================================
REM 1. 清理测试文件
REM ========================================
if %CLEAN_TEST%==1 if %CLEAN_ALL%==0 goto :skip_test_check
:skip_test_check
if %CLEAN_ALL%==1 set CLEAN_TEST=1
if %CLEAN_TEST%==1 (
    echo [1/3] 清理测试临时文件...
    
    REM 测试临时数据
    if exist "tests\*.tmp" del /f /q "tests\*.tmp" 2>nul && echo   - 已删除 tests 目录临时文件
    
    REM 测试用户数据（JSON）
    if exist "data\test_*.json" del /f /q "data\test_*.json" 2>nul && echo   - 已删除测试用户数据
    
    REM 回环测试日志
    if exist "loopback_server.log" del /f /q "loopback_server.log" 2>nul && echo   - 已删除回环测试日志
    
    REM curl 临时文件
    if exist "/tmp/loopback_*.json" del /f /q "/tmp/loopback_*.json" 2>nul
    
    echo   [OK] 测试文件清理完成
    echo.
)

REM ========================================
REM 2. 清理构建产物
REM ========================================
if %CLEAN_ALL%==1 set CLEAN_BUILD=1
if %CLEAN_BUILD%==1 (
    echo [2/3] 清理编译中间文件...
    
    REM 客户端构建
    if exist "client\build" (
        rmdir /s /q "client\build" 2>nul
        echo   - 已删除 client\build 目录
    )
    
    REM 服务端构建
    if exist "server\build" (
        rmdir /s /q "server\build" 2>nul
        echo   - 已删除 server\build 目录
    )
    
    REM CMake 生成文件
    if exist "build" (
        rmdir /s /q "build" 2>nul
        echo   - 已删除 build 目录
    )
    
    REM 对象文件
    if exist "*.o" del /f /q "*.o" 2>nul && echo   - 已删除顶层 .o 文件
    if exist "client\src\*.o" del /f /q "client\src\*.o" 2>nul && echo   - 已删除 client\src\*.o
    if exist "server\src\*.o" del /f /q "server\src\*.o" 2>nul && echo   - 已删除 server\src\*.o
    if exist "server\tools\*.o" del /f /q "server\tools\*.o" 2>nul && echo   - 已删除 server\tools\*.o
    
    REM 可执行文件（测试工具）
    if exist "stress_test.exe" del /f /q "stress_test.exe" 2>nul && echo   - 已删除 stress_test.exe
    
    REM Rust 构建产物
    if exist "server\security\target" (
        rmdir /s /q "server\security\target" 2>nul
        echo   - 已删除 server\security\target (Rust)
    )
    if exist "client\security\target" (
        rmdir /s /q "client\security\target" 2>nul
        echo   - 已删除 client\security\target (Rust)
    )
    
    REM 安装包构建产物
    if exist "installer\*.exe" del /f /q "installer\*.exe" 2>nul && echo   - 已删除安装包
    
    echo   [OK] 构建产物清理完成
    echo.
)

REM ========================================
REM 3. 清理日志和报告
REM ========================================
if %CLEAN_ALL%==1 set CLEAN_LOGS=1
if %CLEAN_LOGS%==1 (
    echo [3/3] 清理日志和报告文件...
    
    REM 测试报告
    if exist "reports" (
        rmdir /s /q "reports" 2>nul
        echo   - 已删除 reports 目录
    )
    
    REM 服务器日志
    if exist "*.log" del /f /q "*.log" 2>nul && echo   - 已删除 .log 文件
    
    echo   [OK] 日志清理完成
    echo.
)

echo ============================================
echo   清理完成
echo   工作目录: %CD%
echo ============================================
echo.
goto :eof

:print_help
echo 用法: cleanup.bat [选项]
echo.
echo 选项:
echo   --all     清理所有内容（默认）
echo   --test    仅清理测试临时文件
echo   --build   仅清理编译中间文件
echo   --logs    仅清理日志和报告
echo   --help    显示此帮助
echo.
echo 示例:
echo   cleanup.bat --all
echo   cleanup.bat --build --logs
echo   cleanup.bat --test
echo.
goto :eof
