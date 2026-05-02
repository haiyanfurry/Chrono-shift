cmake_minimum_required(VERSION 3.15)
project(chrono-client LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找 Rust 编译出的静态库
set(RUST_SECURITY_LIB "${CMAKE_CURRENT_SOURCE_DIR}/security/target/release/libchrono_client_security.a")

# ============================================================
# OpenSSL 检测 (HTTPS 支持需要)
# ============================================================
find_package(OpenSSL QUIET)
if(OpenSSL_FOUND)
    message(STATUS "OpenSSL 已找到: ${OpenSSL_VERSION}")
    add_compile_definitions(HTTPS_SUPPORT=1)
else()
    message(WARNING "OpenSSL 未找到 — HTTPS 支持将被禁用")
    message(WARNING "  请安装 OpenSSL 开发库:")
    message(WARNING "    - MSYS2: pacman -S mingw-w64-x86_64-openssl")
    message(WARNING "    - vcpkg: vcpkg install openssl")
    message(WARNING "    - 或设置 CMAKE_PREFIX_PATH 指向 OpenSSL 安装目录")
    add_compile_definitions(HTTPS_SUPPORT=0)
endif()

# ============================================================
# 源文件
# ============================================================

# === C++ 源文件 (新架构) ===
file(GLOB_RECURSE CLIENT_CPP_SOURCES
    src/util/*.cpp
    src/network/*.cpp
    src/app/*.cpp
    src/storage/*.cpp
    src/security/*.cpp
    src/ai/*.cpp
    src/plugin/*.cpp
    devtools/core/*.cpp
)

# === C 源文件 (旧架构，过渡期保留) ===
file(GLOB CLIENT_C_SOURCES
    src/*.c
    src/network/*.c
)

# === 最终源文件列表 ===
set(CLIENT_SOURCES
    ${CLIENT_CPP_SOURCES}
    ${CLIENT_C_SOURCES}
)

add_executable(chrono-client ${CLIENT_SOURCES})

target_include_directories(chrono-client PRIVATE
    include                          # C 头文件 (含 tls_client.h 等)
    src                              # C++ 头文件 (同目录)
    devtools/core                    # DevToolsEngine/HttpApi/IpcHandler 头文件
    devtools/cli                     # devtools_cli.h 共享头文件
)

# 链接 WebView2
set(WEBVIEW2_LIB "${CMAKE_CURRENT_SOURCE_DIR}/lib/Microsoft.Web.WebView2.Core.dll")

if(EXISTS ${RUST_SECURITY_LIB})
    target_compile_definitions(chrono-client PRIVATE RUST_FEATURE_ENABLED)
    target_link_libraries(chrono-client PRIVATE ${RUST_SECURITY_LIB})
endif()

target_link_libraries(chrono-client PRIVATE
    ws2_32
    user32
    gdi32
    shell32
    ole32
    uuid
)

# HTTPS 支持 — 需要 OpenSSL
if(OpenSSL_FOUND)
    target_link_libraries(chrono-client PRIVATE OpenSSL::SSL OpenSSL::Crypto)
endif()

# Windows GUI 子系统
if(WIN32)
    set_target_properties(chrono-client PROPERTIES
        LINK_FLAGS "/SUBSYSTEM:WINDOWS"
    )
endif()

# ============================================================
# 自定义目标: 生成自签名开发证书
# ============================================================
set(CERTS_DIR "${CMAKE_SOURCE_DIR}/certs")
set(CERT_FILE "${CERTS_DIR}/cert.pem")
set(KEY_FILE  "${CERTS_DIR}/key.pem")

if(WIN32)
    set(GEN_CERT_SCRIPT "${CMAKE_SOURCE_DIR}/gen_cert.bat")
    set(GEN_CERT_CMD call "${GEN_CERT_SCRIPT}" "${CERTS_DIR}")
else()
    set(GEN_CERT_SCRIPT "${CMAKE_SOURCE_DIR}/gen_cert.sh")
    set(GEN_CERT_CMD "${CMAKE_COMMAND}" -E env bash "${GEN_CERT_SCRIPT}" "${CERTS_DIR}")
endif()

add_custom_target(gen-certs
    COMMAND ${GEN_CERT_CMD}
    COMMENT "生成自签名 TLS 开发证书 → ${CERTS_DIR}"
    BYPRODUCTS "${CERT_FILE}" "${KEY_FILE}"
)

add_custom_target(gen-certs-force
    COMMAND ${CMAKE_COMMAND} -E remove "${CERT_FILE}" "${KEY_FILE}"
    COMMAND ${GEN_CERT_CMD}
    COMMENT "强制重新生成自签名 TLS 开发证书 → ${CERTS_DIR}"
)

# 提醒: gen-certs 不是默认构建的一部分
if(NOT EXISTS "${CERT_FILE}")
    message(STATUS "提示: 运行 'cmake --build . --target gen-certs' 生成开发证书")
endif()
