cmake_minimum_required(VERSION 3.15)
project(chrono-server LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ============================================================
# 源文件
# ============================================================
file(GLOB SERVER_SOURCES
    src/*.c
)

add_executable(chrono-server ${SERVER_SOURCES})

target_include_directories(chrono-server PRIVATE include)

# ============================================================
# 平台特定依赖
# ============================================================
if(WIN32)
    # Windows: Winsock 库
    target_link_libraries(chrono-server PRIVATE ws2_32)

    # Windows: Rust 安全库 (可选)
    set(RUST_SECURITY_LIB
        "${CMAKE_CURRENT_SOURCE_DIR}/security/target/release/chrono_server_security.lib")
    if(EXISTS ${RUST_SECURITY_LIB})
        target_link_libraries(chrono-server PRIVATE ${RUST_SECURITY_LIB} userenv advapi32)
        target_compile_definitions(chrono-server PRIVATE HAS_RUST_SECURITY)
    endif()

    # 控制台子系统 (便于调试)
    set_target_properties(chrono-server PROPERTIES
        LINK_FLAGS "/SUBSYSTEM:CONSOLE")

elseif(UNIX)
    # Linux: pthread
    find_package(Threads REQUIRED)
    target_link_libraries(chrono-server PRIVATE Threads::Threads m)

    # Linux: Rust 安全库 (可选)
    set(RUST_SECURITY_LIB
        "${CMAKE_CURRENT_SOURCE_DIR}/security/target/release/libchrono_server_security.a")
    if(EXISTS ${RUST_SECURITY_LIB})
        target_link_libraries(chrono-server PRIVATE ${RUST_SECURITY_LIB})
        target_compile_definitions(chrono-server PRIVATE HAS_RUST_SECURITY)
    endif()
endif()

# ============================================================
# CLI 调试工具
# ============================================================
add_executable(debug_cli tools/debug_cli.c)
target_include_directories(debug_cli PRIVATE include)

if(WIN32)
    target_link_libraries(debug_cli PRIVATE ws2_32)
endif()

# ============================================================
# 编译选项
# ============================================================
if(MSVC)
    target_compile_options(chrono-server PRIVATE /W4)
    target_compile_options(debug_cli PRIVATE /W4)
else()
    target_compile_options(chrono-server PRIVATE -Wall -Wextra -pedantic)
    target_compile_options(debug_cli PRIVATE -Wall -Wextra -pedantic)
endif()
