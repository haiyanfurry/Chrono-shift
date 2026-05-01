/**
 * Chrono-shift 客户端工具函数实现
 */
#include "Utils.h"

#include <cstring>
#include <cstdarg>
#include <algorithm>
#include <fstream>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#include <unistd.h>
#include <sys/types.h>
#define MKDIR(p) mkdir(p, 0755)
#endif

namespace chrono {
namespace client {
namespace util {

std::string trim(const std::string& s) {
    if (s.empty()) return s;
    size_t start = 0;
    while (start < s.size() && (s[start] == ' ' || s[start] == '\t' || s[start] == '\r' || s[start] == '\n'))
        start++;
    size_t end = s.size();
    while (end > start && (s[end-1] == ' ' || s[end-1] == '\t' || s[end-1] == '\r' || s[end-1] == '\n'))
        end--;
    return s.substr(start, end - start);
}

std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> result;
    size_t start = 0;
    for (size_t i = 0; i <= s.size(); i++) {
        if (i == s.size() || s[i] == delim) {
            if (i > start)
                result.push_back(s.substr(start, i - start));
            start = i + 1;
        }
    }
    return result;
}

std::string join(const std::vector<std::string>& parts, const std::string& delim) {
    std::string result;
    for (size_t i = 0; i < parts.size(); i++) {
        if (i > 0) result += delim;
        result += parts[i];
    }
    return result;
}

#ifdef _WIN32
std::string wstring_to_string(const std::wstring& ws) {
    if (ws.empty()) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, ws.data(), (int)ws.size(), nullptr, 0, nullptr, nullptr);
    if (len <= 0) return {};
    std::string result((size_t)len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, ws.data(), (int)ws.size(), &result[0], len, nullptr, nullptr);
    return result;
}

std::wstring string_to_wstring(const std::string& s) {
    if (s.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    if (len <= 0) return {};
    std::wstring result((size_t)len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &result[0], len);
    return result;
}
#else
std::string wstring_to_string(const std::wstring& ws) {
    // 非 Windows 平台简化处理
    std::string result;
    for (wchar_t wc : ws) {
        result.push_back((char)wc);
    }
    return result;
}

std::wstring string_to_wstring(const std::string& s) {
    std::wstring result;
    for (char c : s) {
        result.push_back((wchar_t)(unsigned char)c);
    }
    return result;
}
#endif

bool file_exists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

bool create_directory(const std::string& path) {
    // 递归创建目录
    std::string current;
    auto parts = split(path, '/');
#ifdef _WIN32
    // Windows 下也支持反斜杠
    if (parts.size() <= 1) {
        parts = split(path, '\\');
    }
#endif
    for (const auto& part : parts) {
        if (part.empty()) continue;
        if (!current.empty()) current += '/';
        current += part;
        if (!file_exists(current)) {
            if (MKDIR(current.c_str()) != 0) {
                // 如果目录已存在（竞态条件），忽略错误
                if (!file_exists(current)) return false;
            }
        }
    }
    return true;
}

std::vector<uint8_t> read_file_binary(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return {};
    size_t size = (size_t)file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> data(size);
    file.read(reinterpret_cast<char*>(data.data()), (std::streamsize)size);
    return data;
}

bool write_file_binary(const std::string& path, const uint8_t* data, size_t len) {
    std::ofstream file(path, std::ios::binary);
    if (!file) return false;
    file.write(reinterpret_cast<const char*>(data), (std::streamsize)len);
    return file.good();
}

std::string get_executable_path() {
#ifdef _WIN32
    wchar_t buf[MAX_PATH];
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    return wstring_to_string(buf);
#else
    char buf[4096];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len > 0) {
        buf[len] = '\0';
        return std::string(buf);
    }
    return {};
#endif
}

std::string format_string(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buf[4096];
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    return std::string(buf);
}

} // namespace util
} // namespace client
} // namespace chrono
