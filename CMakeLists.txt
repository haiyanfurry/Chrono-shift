cmake_minimum_required(VERSION 3.15)
project(Chrono-shift LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -pedantic")

add_subdirectory(server)
add_subdirectory(client)
