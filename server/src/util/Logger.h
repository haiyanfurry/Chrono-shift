/**
 * Chrono-shift C++ 日志系统
 * RAII 日志封装，线程安全
 * C++17 重构版
 */
#ifndef CHRONO_CPP_LOGGER_H
#define CHRONO_CPP_LOGGER_H

#include <string>
#include <cstdio>
#include <ctime>
#include <cstdarg>
#include <mutex>
#include <memory>
#include <sstream>

namespace chrono {
namespace util {

enum class LogLevel {
    kDebug = 0,
    kInfo  = 1,
    kWarn  = 2,
    kError = 3
};

/**
 * 日志器 — RAII 方式管理日志输出
 * 线程安全 (mutex 保护)
 */
class Logger {
public:
    static Logger& instance();

    void set_level(LogLevel level);
    LogLevel level() const;

    void debug(const std::string& file, int line, const std::string& msg);
    void info(const std::string& file, int line, const std::string& msg);
    void warn(const std::string& file, int line, const std::string& msg);
    void error(const std::string& file, int line, const std::string& msg);

    // printf 风格格式化日志
    void logf(LogLevel level, const std::string& file, int line,
              const char* fmt, ...) __attribute__((format(printf, 5, 6)));
    void logf(LogLevel level, const std::string& file, int line,
              const char* fmt, va_list args);

    // 便利宏 (使用 __FILE__ 和 __LINE__)
    // 使用完全限定名，确保在任何命名空间下都可正确展开
    #define LOG_DEBUG(...)  ::chrono::util::Logger::instance().logf(::chrono::util::LogLevel::kDebug,  __FILE__, __LINE__, __VA_ARGS__)
    #define LOG_INFO(...)   ::chrono::util::Logger::instance().logf(::chrono::util::LogLevel::kInfo,   __FILE__, __LINE__, __VA_ARGS__)
    #define LOG_WARN(...)   ::chrono::util::Logger::instance().logf(::chrono::util::LogLevel::kWarn,   __FILE__, __LINE__, __VA_ARGS__)
    #define LOG_ERROR(...)  ::chrono::util::Logger::instance().logf(::chrono::util::LogLevel::kError,  __FILE__, __LINE__, __VA_ARGS__)

private:
    Logger() = default;
    ~Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    LogLevel level_ = LogLevel::kInfo;
    std::mutex mutex_;

    static const char* level_name(LogLevel level);
    static std::string filename_only(const std::string& path);
    static std::string current_timestamp();
};

} // namespace util
} // namespace chrono

#endif // CHRONO_CPP_LOGGER_H
