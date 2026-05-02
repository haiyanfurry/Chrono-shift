/**
 * Chrono-shift 客户端日志系统实现
 */
#include "Logger.h"

#include <cstring>
#include <algorithm>

namespace chrono {
namespace client {
namespace util {

static const char* level_name(LogLevel level) {
    switch (level) {
        case LogLevel::kDebug: return "DEBUG";
        case LogLevel::kInfo:  return "INFO";
        case LogLevel::kWarn:  return "WARN";
        case LogLevel::kError: return "ERROR";
        default:               return "UNKNOWN";
    }
}

Logger& Logger::instance() {
    static Logger s_instance;
    return s_instance;
}

Logger::Logger()
    : level_(LogLevel::kInfo)
    , quiet_(false)
    , file_(nullptr)
{
}

Logger::~Logger() {
    if (file_) {
        fclose(file_);
    }
}

void Logger::set_level(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    level_ = level;
}

LogLevel Logger::level() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return level_;
}

void Logger::set_output_file(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_) {
        fclose(file_);
    }
    file_ = fopen(path.c_str(), "a");
}

void Logger::set_quiet(bool quiet) {
    std::lock_guard<std::mutex> lock(mutex_);
    quiet_ = quiet;
}

void Logger::log(LogLevel level, const char* file, int line, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vlog(level, file, line, fmt, args);
    va_end(args);
}

void Logger::logf(LogLevel level, const char* file, int line, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vlog(level, file, line, fmt, args);
    va_end(args);
}

void Logger::vlog(LogLevel level, const char* file, int line, const char* fmt, va_list args) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (level < level_) return;

    // 时间戳
    auto now = std::chrono::system_clock::now();
    auto now_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count() % 1000;

    struct tm tm_buf;
#ifdef _WIN32
    localtime_s(&tm_buf, &now_t);
#else
    localtime_r(&now_t, &tm_buf);
#endif

    char time_buf[32];
    strftime(time_buf, sizeof(time_buf), "%H:%M:%S", &tm_buf);

    // 提取文件名
    const char* basename = file;
    if (const char* p = strrchr(file, '/')) basename = p + 1;
    if (const char* p = strrchr(basename, '\\')) basename = p + 1;

    // 格式化消息
    char msg_buf[4096];
    vsnprintf(msg_buf, sizeof(msg_buf), fmt, args);

    // 输出到 stderr
    if (!quiet_) {
        fprintf(stderr, "[%s.%03lld][%s] %s:%d | %s\n",
                time_buf, (long long)ms,
                level_name(level), basename, line, msg_buf);
        fflush(stderr);
    }

    // 输出到文件
    if (file_) {
        fprintf(file_, "[%s.%03lld][%s] %s:%d | %s\n",
                time_buf, (long long)ms,
                level_name(level), basename, line, msg_buf);
        fflush(file_);
    }
}

} // namespace util
} // namespace client
} // namespace chrono
