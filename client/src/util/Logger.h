/**
 * Chrono-shift 客户端日志系统
 * C++17 重构版 — 自包含实现，不依赖服务端代码
 */
#ifndef CHRONO_CLIENT_LOGGER_H
#define CHRONO_CLIENT_LOGGER_H

#include <string>
#include <cstdio>
#include <cstdarg>
#include <mutex>
#include <chrono>
#include <ctime>

namespace chrono {
namespace client {
namespace util {

/**
 * 日志级别
 */
enum class LogLevel {
    kDebug = 0,
    kInfo  = 1,
    kWarn  = 2,
    kError = 3
};

/**
 * 日志器 — 线程安全单例
 */
class Logger {
public:
    static Logger& instance();

    void set_level(LogLevel level);
    LogLevel level() const;

    void log(LogLevel level, const char* file, int line, const char* fmt, ...);
    void logf(LogLevel level, const char* file, int line, const char* fmt, ...);

    void set_output_file(const std::string& path);
    void set_quiet(bool quiet);

private:
    Logger();
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void vlog(LogLevel level, const char* file, int line, const char* fmt, va_list args);

    LogLevel level_ = LogLevel::kInfo;
    bool quiet_ = false;
    FILE* file_ = nullptr;
    mutable std::mutex mutex_;
};

// 宏定义
#ifndef LOG_DEBUG
#define LOG_DEBUG(...)  ::chrono::client::util::Logger::instance().logf(::chrono::client::util::LogLevel::kDebug,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...)   ::chrono::client::util::Logger::instance().logf(::chrono::client::util::LogLevel::kInfo,   __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN(...)   ::chrono::client::util::Logger::instance().logf(::chrono::client::util::LogLevel::kWarn,   __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...)  ::chrono::client::util::Logger::instance().logf(::chrono::client::util::LogLevel::kError,  __FILE__, __LINE__, __VA_ARGS__)
#endif

} // namespace util
} // namespace client
} // namespace chrono

#endif // CHRONO_CLIENT_LOGGER_H
