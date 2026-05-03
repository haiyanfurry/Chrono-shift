cmake_minimum_required(VERSION 3.15)
project(Chrono-shift LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ============================================================
# Chrono-shift 根项目 CMakeLists.txt
# 子项目可独立构建:
#   cd server && cmake -B build && cmake --build build
#   cd client && cmake -B build && cmake --build build
# ============================================================

# 编译选项
if(MSVC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W4")
else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -pedantic")
endif()

# 子项目 (仅客户端，服务端已移除)
add_subdirectory(client)
