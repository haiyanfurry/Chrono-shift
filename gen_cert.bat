@echo off
REM ============================================================
REM 墨竹 (Chrono-shift) 自签名 TLS 证书生成脚本 (Windows)
REM
REM 用途: 快速生成开发用自签名 RSA 2048 证书
REM 用法: gen_cert.bat [output_dir] [cn] [days]
REM   output_dir  证书输出目录 (默认: ./certs)
REM   cn          证书 Common Name (默认: 127.0.0.1)
REM   days        证书有效天数 (默认: 3650, 即 10 年)
REM
REM 输出:
REM   output_dir/cert.pem  — 自签名证书 (含 SAN)
REM   output_dir/key.pem   — RSA 私钥
REM
REM 依赖: OpenSSL
REM   优先使用 D:\mys32\bin\openssl.exe (MSYS2)
REM   其次使用 PATH 中的 openssl
REM ============================================================
setlocal enabledelayedexpansion

set "SCRIPT_DIR=%~dp0"
cd /d "%SCRIPT_DIR%"

REM --- 参数 ---
set "OUTPUT_DIR=%~1"
if "%OUTPUT_DIR%"=="" set "OUTPUT_DIR=./certs"

set "CN=%~2"
if "%CN%"=="" set "CN=127.0.0.1"

set "DAYS=%~3"
if "%DAYS%"=="" set "DAYS=3650"

set "CERT_FILE=%OUTPUT_DIR%\cert.pem"
set "KEY_FILE=%OUTPUT_DIR%\key.pem"

REM --- 检测 OpenSSL ---
set "OPENSSL_CMD=openssl"

REM 优先使用 MSYS2 OpenSSL
if exist "D:\mys32\bin\openssl.exe" (
    set "OPENSSL_CMD=D:\mys32\bin\openssl.exe"
    echo [INFO] 使用 MSYS2 OpenSSL: D:\mys32\bin\openssl.exe
) else (
    where openssl >nul 2>nul
    if !ERRORLEVEL! neq 0 (
        echo [ERROR] 未找到 OpenSSL！
        echo.
        echo 请安装 OpenSSL 或将 openssl.exe 添加到 PATH：
        echo   - MSYS2: 安装于 D:\mys32\bin\openssl.exe
        echo   - 或使用包管理器安装: pacman -S openssl
        echo.
        pause
        exit /b 1
    )
    echo [INFO] 使用系统 PATH 中的 OpenSSL
)

REM --- 检查是否已存在 ---
if exist "%CERT_FILE%" (
    echo [WARN] 证书文件已存在: %CERT_FILE%
    echo        请先删除现有文件，或指定不同的输出目录。
    set /p "OVERWRITE=是否覆盖? (y/N): "
    if /i not "!OVERWRITE!"=="y" (
        echo [INFO] 已取消
        exit /b 0
    )
)
if exist "%KEY_FILE%" (
    echo [WARN] 私钥文件已存在: %KEY_FILE%
    set /p "OVERWRITE=是否覆盖? (y/N): "
    if /i not "!OVERWRITE!"=="y" (
        echo [INFO] 已取消
        exit /b 0
    )
)

REM --- 创建输出目录 ---
if not exist "%OUTPUT_DIR%" (
    mkdir "%OUTPUT_DIR%"
    if !ERRORLEVEL! neq 0 (
        echo [ERROR] 创建输出目录失败: %OUTPUT_DIR%
        pause
        exit /b 1
    )
)

echo.
echo ============================================
echo   墨竹 自签名证书生成工具
echo ============================================
echo   输出目录: %OUTPUT_DIR%
echo   Common Name: %CN%
echo   有效期: %DAYS% 天
echo   OpenSSL: %OPENSSL_CMD%
echo ============================================
echo.

REM --- Step 1: 生成 RSA 私钥 ---
echo [1/3] 生成 RSA 2048 位私钥...
"%OPENSSL_CMD%" genrsa -out "%KEY_FILE%" 2048
if !ERRORLEVEL! neq 0 (
    echo [ERROR] 私钥生成失败
    pause
    exit /b 1
)
echo [OK] 私钥已生成: %KEY_FILE%

REM --- Step 2: 生成自签名证书 (含 SAN) ---
echo [2/3] 生成自签名证书 (SHA-256, SAN)...

REM 创建 OpenSSL 配置文件片段用于 SAN
set "SAN_CONFIG=%TEMP%\chrono_san.cnf"
(
    echo [req]
    echo distinguished_name = req_distinguished_name
    echo x509_extensions = v3_req
    echo prompt = no
    echo.
    echo [req_distinguished_name]
    echo CN = %CN%
    echo.
    echo [v3_req]
    echo keyUsage = keyEncipherment, dataEncipherment, digitalSignature
    echo extendedKeyUsage = serverAuth
    echo subjectAltName = @alt_names
    echo.
    echo [alt_names]
    echo DNS.1 = %CN%
    echo IP.1 = 127.0.0.1
    echo IP.2 = ::1
) > "%SAN_CONFIG%"

"%OPENSSL_CMD%" req -x509 -nodes -days %DAYS% -newkey rsa:2048 ^
    -keyout "%KEY_FILE%" -out "%CERT_FILE%" ^
    -config "%SAN_CONFIG%" ^
    -sha256
if !ERRORLEVEL! neq 0 (
    echo [ERROR] 证书生成失败
    del "%SAN_CONFIG%" >nul 2>nul
    pause
    exit /b 1
)
del "%SAN_CONFIG%" >nul 2>nul
echo [OK] 证书已生成: %CERT_FILE%

REM --- Step 3: 验证证书 ---
echo [3/3] 验证证书...
"%OPENSSL_CMD%" x509 -in "%CERT_FILE%" -text -noout >nul 2>nul
if !ERRORLEVEL! neq 0 (
    echo [WARN] 证书验证失败，但文件可能已生成
) else (
    "%OPENSSL_CMD%" x509 -in "%CERT_FILE%" -noout -subject -dates
    echo [OK] 证书验证通过
)

echo.
echo ============================================
echo   证书生成完成！
echo ============================================
echo   证书: %CERT_FILE%
echo   私钥: %KEY_FILE%
echo.
echo   使用方式:
echo     - 客户端: 将 cert.pem 和 key.pem 放在客户端运行目录
echo     - CLI:    运行 chrono-devtools gen-cert
echo     - 开发:   添加 --ignore-certificate-errors 浏览器参数
echo ============================================
echo.

exit /b 0
