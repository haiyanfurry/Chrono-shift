cmake_minimum_required(VERSION 3.15)
project(chrono-client LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找 Rust 编译出的静态库
set(RUST_SECURITY_LIB "${CMAKE_CURRENT_SOURCE_DIR}/security/target/release/libchrono_client_security.a")

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

# Windows GUI 子系统
if(WIN32)
    set_target_properties(chrono-client PROPERTIES
        LINK_FLAGS "/SUBSYSTEM:WINDOWS"
    )
endif()
