cmake_minimum_required(VERSION 3.15)
project(chrono-server LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================================
# C++ 源文件 (新重构版)
# ============================================================
file(GLOB_RECURSE CPP_SOURCES
    src/json/JsonValue.cpp
    src/json/JsonParser.cpp
    src/util/Logger.cpp
    src/util/StringUtils.cpp
    src/util/Protocol.cpp
    src/http/HttpTypes.cpp
    src/http/HttpParser.cpp
    src/http/HttpServer.cpp
    src/tls/TlsContext.cpp
    src/db/Database.cpp
    src/ffi/RustBridge.cpp
    src/ws/WebSocket.cpp
    src/security/SecurityManager.cpp
    src/security/OAuthClient.cpp
    src/security/EmailVerifier.cpp
    src/handler/UserHandler.cpp
    src/handler/MessageHandler.cpp
    src/handler/OAuthHandler.cpp
    src/main_cpp.cpp
)

add_executable(chrono-server-cpp ${CPP_SOURCES})

target_include_directories(chrono-server-cpp PRIVATE
    include
    src
)

target_compile_definitions(chrono-server-cpp PRIVATE HAS_TLS)

# 强制 OpenSSL 依赖
find_package(OpenSSL REQUIRED)
target_link_libraries(chrono-server-cpp PRIVATE OpenSSL::SSL OpenSSL::Crypto)

# ============================================================
# 平台特定依赖
# ============================================================
if(WIN32)
    # Windows: Winsock 库
    target_link_libraries(chrono-server-cpp PRIVATE ws2_32)

    # Windows: Rust 安全库 (可选)
    set(RUST_SECURITY_LIB
        "${CMAKE_CURRENT_SOURCE_DIR}/security/target/release/chrono_server_security.lib")
    if(EXISTS ${RUST_SECURITY_LIB})
        target_link_libraries(chrono-server-cpp PRIVATE ${RUST_SECURITY_LIB} userenv advapi32)
        target_compile_definitions(chrono-server-cpp PRIVATE HAS_RUST_SECURITY)
    endif()

    # 控制台子系统
    set_target_properties(chrono-server-cpp PROPERTIES
        LINK_FLAGS "/SUBSYSTEM:CONSOLE")

elseif(UNIX)
    # Linux: pthread
    find_package(Threads REQUIRED)
    target_link_libraries(chrono-server-cpp PRIVATE Threads::Threads m)

    # Linux: Rust 安全库 (可选)
    set(RUST_SECURITY_LIB
        "${CMAKE_CURRENT_SOURCE_DIR}/security/target/release/libchrono_server_security.a")
    if(EXISTS ${RUST_SECURITY_LIB})
        target_link_libraries(chrono-server-cpp PRIVATE ${RUST_SECURITY_LIB})
        target_compile_definitions(chrono-server-cpp PRIVATE HAS_RUST_SECURITY)
    endif()
endif()

# ============================================================
# C 源文件 (旧版, 保留用于过渡)
# ============================================================
file(GLOB SERVER_C_SOURCES
    src/*.c
)
list(REMOVE_ITEM SERVER_C_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/tls_stub.c")

add_executable(chrono-server ${SERVER_C_SOURCES})

target_include_directories(chrono-server PRIVATE include)
target_compile_definitions(chrono-server PRIVATE HAS_TLS)

target_link_libraries(chrono-server PRIVATE OpenSSL::SSL OpenSSL::Crypto)

if(WIN32)
    target_link_libraries(chrono-server PRIVATE ws2_32)

    set(RUST_SECURITY_LIB
        "${CMAKE_CURRENT_SOURCE_DIR}/security/target/release/chrono_server_security.lib")
    if(EXISTS ${RUST_SECURITY_LIB})
        target_link_libraries(chrono-server PRIVATE ${RUST_SECURITY_LIB} userenv advapi32)
        target_compile_definitions(chrono-server PRIVATE HAS_RUST_SECURITY)
    endif()

    set_target_properties(chrono-server PROPERTIES
        LINK_FLAGS "/SUBSYSTEM:CONSOLE")

elseif(UNIX)
    find_package(Threads REQUIRED)
    target_link_libraries(chrono-server PRIVATE Threads::Threads m)

    set(RUST_SECURITY_LIB
        "${CMAKE_CURRENT_SOURCE_DIR}/security/target/release/libchrono_server_security.a")
    if(EXISTS ${RUST_SECURITY_LIB})
        target_link_libraries(chrono-server PRIVATE ${RUST_SECURITY_LIB})
        target_compile_definitions(chrono-server PRIVATE HAS_RUST_SECURITY)
    endif()
endif()

# ============================================================
# CLI 调试工具 (旧 C 版)
# ============================================================
add_executable(debug_cli tools/debug_cli.c src/json_parser.c src/tls_server.c src/platform_compat.c)
target_include_directories(debug_cli PRIVATE include)
target_compile_definitions(debug_cli PRIVATE HAS_TLS)
target_link_libraries(debug_cli PRIVATE OpenSSL::SSL OpenSSL::Crypto)

if(WIN32)
    target_link_libraries(debug_cli PRIVATE ws2_32)
endif()

# ============================================================
# 单元测试 (Unity 框架)
# ============================================================
add_executable(chrono-test
    tests/test_runner.c
    tests/test_json_parser.c
    tests/test_http_parse.c
    src/json_parser.c
    src/http_parse.c
)
target_include_directories(chrono-test PRIVATE include tests)
target_compile_definitions(chrono-test PRIVATE UNIT_TESTING)
target_compile_options(chrono-test PRIVATE -Wall -Wextra -pedantic)
target_link_libraries(chrono-test PRIVATE m)

# ============================================================
# 编译选项
# ============================================================
if(MSVC)
    target_compile_options(chrono-server-cpp PRIVATE /W4)
    target_compile_options(chrono-server PRIVATE /W4)
    target_compile_options(debug_cli PRIVATE /W4)
else()
    target_compile_options(chrono-server-cpp PRIVATE -Wall -Wextra -pedantic)
    target_compile_options(chrono-server PRIVATE -Wall -Wextra -pedantic)
    target_compile_options(debug_cli PRIVATE -Wall -Wextra -pedantic)
endif()

message(STATUS "C++ server target: chrono-server-cpp")
message(STATUS "C (legacy) server target: chrono-server")
