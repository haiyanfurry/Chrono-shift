/**
 * Chrono-shift C++ 字符串工具
 * UUID 生成、输入验证、时间戳
 * C++17 重构版
 */
#ifndef CHRONO_CPP_STRING_UTILS_H
#define CHRONO_CPP_STRING_UTILS_H

#include <string>
#include <cstdint>
#include <vector>
#include <optional>

namespace chrono {
namespace util {

class StringUtils {
public:
    // 删除拷贝构造
    StringUtils() = delete;

    /**
     * 生成 UUID v4 (简单实现)
     * @return 36字符 UUID 字符串 (xxxxxxxx-xxxx-4xxx-xxxx-xxxxxxxxxxxx)
     */
    static std::string generate_uuid();

    /**
     * 获取当前时间戳 (毫秒)
     */
    static uint64_t timestamp_ms();

    /**
     * 输入验证 — 检查字符串是否安全
     * 禁止控制字符 (保留 tab/newline/cr) 和 DEL
     * @return true 如果输入安全
     */
    static bool is_input_safe(const std::string& input, size_t max_len = 8192);

    /**
     * URL 编码
     */
    static std::string url_encode(const std::string& input);

    /**
     * URL 解码
     */
    static std::string url_decode(const std::string& input);

    /**
     * Base64 编码
     */
    static std::string base64_encode(const std::vector<uint8_t>& data);
    static std::string base64_encode(const std::string& input);

    /**
     * Base64 解码
     */
    static std::optional<std::vector<uint8_t>> base64_decode(const std::string& input);

    /**
     * 字符串分割
     */
    static std::vector<std::string> split(const std::string& str, char delimiter);

    /**
     * 字符串修剪
     */
    static std::string trim(const std::string& str);

    /**
     * 字符串替换
     */
    static std::string replace(const std::string& str, const std::string& from, const std::string& to);

    /**
     * 转换为小写
     */
    static std::string to_lower(const std::string& str);

    /**
     * 十六进制编码
     */
    static std::string hex_encode(const std::vector<uint8_t>& data);

    /**
     * 十六进制解码
     */
    static std::optional<std::vector<uint8_t>> hex_decode(const std::string& hex);
};

} // namespace util
} // namespace chrono

#endif // CHRONO_CPP_STRING_UTILS_H
