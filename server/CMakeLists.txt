cmake_minimum_required(VERSION 3.15)
project(chrono-server LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 查找 Rust 编译出的静态库
set(RUST_SECURITY_LIB "${CMAKE_CURRENT_SOURCE_DIR}/security/target/release/libchrono_server_security.a")

file(GLOB SERVER_SOURCES
    src/*.c
)

add_executable(chrono-server ${SERVER_SOURCES})

target_include_directories(chrono-server PRIVATE include)

if(EXISTS ${RUST_SECURITY_LIB})
    target_link_libraries(chrono-server PRIVATE ${RUST_SECURITY_LIB})
    target_link_libraries(chrono-server PRIVATE ws2_32 userenv advapi32)
endif()

# Windows 子系统设置（控制台应用，便于调试）
if(WIN32)
    set_target_properties(chrono-server PROPERTIES LINK_FLAGS "/SUBSYSTEM:CONSOLE")
endif()
