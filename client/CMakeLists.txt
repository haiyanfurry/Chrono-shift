cmake_minimum_required(VERSION 3.15)
project(chrono-client LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 查找 Rust 编译出的静态库
set(RUST_SECURITY_LIB "${CMAKE_CURRENT_SOURCE_DIR}/security/target/release/libchrono_client_security.a")

file(GLOB CLIENT_SOURCES
    src/*.c
)

add_executable(chrono-client ${CLIENT_SOURCES})

target_include_directories(chrono-client PRIVATE include)

# 链接 WebView2
set(WEBVIEW2_LIB "${CMAKE_CURRENT_SOURCE_DIR}/lib/Microsoft.Web.WebView2.Core.dll")

if(EXISTS ${RUST_SECURITY_LIB})
    target_link_libraries(chrono-client PRIVATE ${RUST_SECURITY_LIB})
    target_link_libraries(chrono-client PRIVATE ws2_32 user32 gdi32 shell32 ole32 uuid)
endif()

# Windows GUI 子系统
if(WIN32)
    set_target_properties(chrono-client PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS")
endif()
