/**
 * Chrono-shift 客户端工具函数
 * C++17 重构版
 */
#ifndef CHRONO_CLIENT_UTILS_H
#define CHRONO_CLIENT_UTILS_H

#include <cstdint>
#include <string>
#include <vector>

namespace chrono {
namespace client {
namespace util {

/** 去除字符串首尾空白 */
std::string trim(const std::string& s);

/** 分割字符串 */
std::vector<std::string> split(const std::string& s, char delim);

/** 连接字符串 */
std::string join(const std::vector<std::string>& parts, const std::string& delim);

/** Win32 宽字符转 UTF-8 */
std::string wstring_to_string(const std::wstring& ws);

/** UTF-8 转 Win32 宽字符 */
std::wstring string_to_wstring(const std::string& s);

/** 检查文件是否存在 */
bool file_exists(const std::string& path);

/** 创建目录（递归） */
bool create_directory(const std::string& path);

/** 读取文件全部内容 */
std::vector<uint8_t> read_file_binary(const std::string& path);

/** 写入二进制文件 */
bool write_file_binary(const std::string& path, const uint8_t* data, size_t len);

/** 获取当前可执行文件路径 */
std::string get_executable_path();

/** 格式化字符串 (sprintf 安全封装) */
std::string format_string(const char* fmt, ...);

} // namespace util
} // namespace client
} // namespace chrono

#endif // CHRONO_CLIENT_UTILS_H
