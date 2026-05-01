/**
 * Chrono-shift C++ 日志系统实现
 */
#include "Logger.h"
#include <cstring>
#include <cstdio>
#include <ctime>
#include <chrono>

namespace chrono {
namespace util {

Logger& Logger::instance() {
    static Logger instance;
    return instance;
}

void Logger::set_level(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    level_ = level;
}

LogLevel Logger::level() const {
    return level_;
}

const char* Logger::level_name(LogLevel level) {
    switch (level) {
        case LogLevel::kDebug: return "DEBUG";
        case LogLevel::kInfo:  return "INFO";
        case LogLevel::kWarn:  return "WARN";
        case LogLevel::kError: return "ERROR";
        default: return "UNKNOWN";
    }
}

std::string Logger::filename_only(const std::string& path) {
    auto pos = path.find_last_of("/\\");
    if (pos != std::string::npos) {
        return path.substr(pos + 1);
    }
    return path;
}

std::string Logger::current_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto tt = std::chrono::system_clock::to_time_t(now);
    char buf[32] = {};
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&tt));
    return std::string(buf);
}

void Logger::debug(const std::string& file, int line, const std::string& msg) {
    logf(LogLevel::kDebug, file, line, "%s", msg.c_str());
}

void Logger::info(const std::string& file, int line, const std::string& msg) {
    logf(LogLevel::kInfo, file, line, "%s", msg.c_str());
}

void Logger::warn(const std::string& file, int line, const std::string& msg) {
    logf(LogLevel::kWarn, file, line, "%s", msg.c_str());
}

void Logger::error(const std::string& file, int line, const std::string& msg) {
    logf(LogLevel::kError, file, line, "%s", msg.c_str());
}

void Logger::logf(LogLevel level, const std::string& file, int line,
                   const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    logf(level, file, line, fmt, args);
    va_end(args);
}

void Logger::logf(LogLevel level, const std::string& file, int line,
                   const char* fmt, va_list args) {
    std::string msg;
    {
        va_list args_copy;
        va_copy(args_copy, args);
        int needed = vsnprintf(nullptr, 0, fmt, args_copy);
        va_end(args_copy);
        if (needed >= 0) {
            msg.resize(static_cast<size_t>(needed));
            vsnprintf(&msg[0], msg.size() + 1, fmt, args);
        }
    }

    std::lock_guard<std::mutex> lock(mutex_);

    if (level < level_) return;

    std::string ts = current_timestamp();
    std::string fn = filename_only(file);

    printf("[%s] [%s] [%s:%d] %s\n",
           ts.c_str(), level_name(level), fn.c_str(), line, msg.c_str());
    fflush(stdout);
}

} // namespace util
} // namespace chrono
