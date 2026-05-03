/**
 * print_compat.h — std::println/std::print 替代实现
 *
 * MinGW GCC 15 的 <print> 头在 Windows 上需要内部终端运行时符号
 * (__open_terminal / __write_to_terminal)，这些符号在独立构建中不可用。
 *
 * 本头文件提供基于 std::format + std::fputs 的兼容实现，
 * 放置于 chrono::client::cli 命名空间中，完全兼容 C++23 std::format 语法。
 *
 * 使用方式:
 *   #include "print_compat.h"   // 替代 #include <print>
 *   cli::println("Hello {}!", name);
 *   cli::println(stderr, "Error: {}", msg);
 *   cli::print("no newline");
 */

#ifndef CHRONO_PRINT_COMPAT_H
#define CHRONO_PRINT_COMPAT_H

#include <format>      // std::format, std::format_string
#include <cstdio>      // std::fputs, std::fputc, FILE, stdout, stderr

namespace chrono::client::cli {

// ---------------------------------------------------------------------------
// println — 输出并换行
// ---------------------------------------------------------------------------

/// 无参数换行
inline void println() noexcept {
    std::fputc('\n', stdout);
}

/// 格式化输出到 stdout + 换行
template<typename... Args>
void println(std::format_string<Args...> fmt, Args&&... args) {
    auto s = std::format(fmt, std::forward<Args>(args)...);
    std::fputs(s.c_str(), stdout);
    std::fputc('\n', stdout);
}

/// 格式化输出到指定流 + 换行
template<typename... Args>
void println(FILE* stream, std::format_string<Args...> fmt, Args&&... args) {
    auto s = std::format(fmt, std::forward<Args>(args)...);
    std::fputs(s.c_str(), stream);
    std::fputc('\n', stream);
}

// ---------------------------------------------------------------------------
// print  — 输出不换行
// ---------------------------------------------------------------------------

/// 格式化输出到 stdout (不换行)
template<typename... Args>
void print(std::format_string<Args...> fmt, Args&&... args) {
    auto s = std::format(fmt, std::forward<Args>(args)...);
    std::fputs(s.c_str(), stdout);
}

/// 格式化输出到指定流 (不换行)
template<typename... Args>
void print(FILE* stream, std::format_string<Args...> fmt, Args&&... args) {
    auto s = std::format(fmt, std::forward<Args>(args)...);
    std::fputs(s.c_str(), stream);
}

} // namespace chrono::client::cli

#endif /* CHRONO_PRINT_COMPAT_H */
