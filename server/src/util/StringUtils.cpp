/**
 * Chrono-shift C++ 字符串工具实现
 */
#include "StringUtils.h"
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <random>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <algorithm>

namespace chrono {
namespace util {

// ============================================================
// UUID v4 生成
// ============================================================

std::string StringUtils::generate_uuid() {
    static std::mt19937_64 rng(std::chrono::steady_clock::now().time_since_epoch().count());
    static const char hex[] = "0123456789abcdef";

    std::string uuid(36, '-');
    for (int i = 0; i < 36; i++) {
        if (i == 8 || i == 13 || i == 18 || i == 23) continue; // 保留 '-'
        if (i == 14) {
            uuid[i] = '4'; // UUID v4 版本位
        } else if (i == 19) {
            // UUID v4 变体位: 8, 9, a, b
            uuid[i] = hex[8 + (rng() % 4)];
        } else {
            uuid[i] = hex[rng() % 16];
        }
    }
    return uuid;
}

// ============================================================
// 时间戳
// ============================================================

uint64_t StringUtils::timestamp_ms() {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    return static_cast<uint64_t>(ms);
}

// ============================================================
// 输入验证
// ============================================================

bool StringUtils::is_input_safe(const std::string& input, size_t max_len) {
    if (input.empty() || input.length() > max_len) return false;

    for (unsigned char c : input) {
        // 允许 tab, newline, cr 和可打印字符
        if (c < 0x20 && c != '\t' && c != '\n' && c != '\r') return false;
        if (c == 0x7F) return false; // DEL
    }
    return true;
}

// ============================================================
// URL 编解码
// ============================================================

std::string StringUtils::url_encode(const std::string& input) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (unsigned char c : input) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else {
            escaped << '%' << std::setw(2) << static_cast<int>(c);
        }
    }
    return escaped.str();
}

std::string StringUtils::url_decode(const std::string& input) {
    std::string result;
    result.reserve(input.length());

    for (size_t i = 0; i < input.length(); i++) {
        if (input[i] == '%' && i + 2 < input.length()) {
            unsigned int val;
            std::istringstream is(input.substr(i + 1, 2));
            if (is >> std::hex >> val) {
                result += static_cast<char>(val);
                i += 2;
            } else {
                result += input[i];
            }
        } else if (input[i] == '+') {
            result += ' ';
        } else {
            result += input[i];
        }
    }
    return result;
}

// ============================================================
// Base64 编解码
// ============================================================

static const char kBase64Chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string StringUtils::base64_encode(const std::vector<uint8_t>& data) {
    std::string result;
    result.reserve(((data.size() + 2) / 3) * 4);

    size_t i = 0;
    while (i + 3 <= data.size()) {
        uint32_t triple = (static_cast<uint32_t>(data[i]) << 16) |
                          (static_cast<uint32_t>(data[i + 1]) << 8) |
                          static_cast<uint32_t>(data[i + 2]);
        result += kBase64Chars[(triple >> 18) & 0x3F];
        result += kBase64Chars[(triple >> 12) & 0x3F];
        result += kBase64Chars[(triple >> 6) & 0x3F];
        result += kBase64Chars[triple & 0x3F];
        i += 3;
    }

    if (i + 1 == data.size()) {
        uint32_t triple = static_cast<uint32_t>(data[i]) << 16;
        result += kBase64Chars[(triple >> 18) & 0x3F];
        result += kBase64Chars[(triple >> 12) & 0x3F];
        result += "==";
    } else if (i + 2 == data.size()) {
        uint32_t triple = (static_cast<uint32_t>(data[i]) << 16) |
                          (static_cast<uint32_t>(data[i + 1]) << 8);
        result += kBase64Chars[(triple >> 18) & 0x3F];
        result += kBase64Chars[(triple >> 12) & 0x3F];
        result += kBase64Chars[(triple >> 6) & 0x3F];
        result += "=";
    }

    return result;
}

std::string StringUtils::base64_encode(const std::string& input) {
    std::vector<uint8_t> data(input.begin(), input.end());
    return base64_encode(data);
}

std::optional<std::vector<uint8_t>> StringUtils::base64_decode(
    const std::string& input) {
    // 构建反向查找表
    static const uint8_t kDecodeTable[256] = {
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x3E,0xFF,0xFF,0xFF,0x3F,
        0x34,0x35,0x36,0x37,0x38,0x39,0x3A,0x3B,0x3C,0x3D,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
        0xFF,0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,
        0x0F,0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0xFF,0xFF,0xFF,0xFF,0xFF,
        0xFF,0x1A,0x1B,0x1C,0x1D,0x1E,0x1F,0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,
        0x29,0x2A,0x2B,0x2C,0x2D,0x2E,0x2F,0x30,0x31,0x32,0x33,0xFF,0xFF,0xFF,0xFF,0xFF,
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF
    };

    if (input.empty()) return std::vector<uint8_t>();

    // 计算有效字符数和填充
    size_t padding = 0;
    if (input.length() > 0 && input[input.length() - 1] == '=') padding++;
    if (input.length() > 1 && input[input.length() - 2] == '=') padding++;

    size_t data_len = (input.length() * 3) / 4 - padding;
    std::vector<uint8_t> result;
    result.reserve(data_len);

    uint32_t buffer = 0;
    int bits_collected = 0;

    for (unsigned char c : input) {
        if (c == '=') break;
        uint8_t val = kDecodeTable[c];
        if (val == 0xFF) return std::nullopt; // 无效字符

        buffer = (buffer << 6) | val;
        bits_collected += 6;

        if (bits_collected >= 24) {
            bits_collected -= 24;
            result.push_back(static_cast<uint8_t>((buffer >> 16) & 0xFF));
            result.push_back(static_cast<uint8_t>((buffer >> 8) & 0xFF));
            result.push_back(static_cast<uint8_t>(buffer & 0xFF));
            buffer = 0;
        }
    }

    // 处理剩余字节
    if (bits_collected == 12) {
        buffer >>= 4;
        result.push_back(static_cast<uint8_t>(buffer & 0xFF));
    } else if (bits_collected == 18) {
        buffer >>= 2;
        result.push_back(static_cast<uint8_t>((buffer >> 8) & 0xFF));
        result.push_back(static_cast<uint8_t>(buffer & 0xFF));
    }

    return result;
}

// ============================================================
// 字符串操作
// ============================================================

std::vector<std::string> StringUtils::split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream stream(str);
    while (std::getline(stream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

std::string StringUtils::trim(const std::string& str) {
    auto start = str.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    auto end = str.find_last_not_of(" \t\n\r");
    return str.substr(start, end - start + 1);
}

std::string StringUtils::replace(const std::string& str, const std::string& from,
                                  const std::string& to) {
    std::string result = str;
    size_t pos = 0;
    while ((pos = result.find(from, pos)) != std::string::npos) {
        result.replace(pos, from.length(), to);
        pos += to.length();
    }
    return result;
}

std::string StringUtils::to_lower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

// ============================================================
// 十六进制编解码
// ============================================================

std::string StringUtils::hex_encode(const std::vector<uint8_t>& data) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (auto byte : data) {
        oss << std::setw(2) << static_cast<int>(byte);
    }
    return oss.str();
}

std::optional<std::vector<uint8_t>> StringUtils::hex_decode(const std::string& hex) {
    if (hex.length() % 2 != 0) return std::nullopt;
    std::vector<uint8_t> result;
    result.reserve(hex.length() / 2);
    for (size_t i = 0; i < hex.length(); i += 2) {
        unsigned int byte;
        std::istringstream iss(hex.substr(i, 2));
        if (!(iss >> std::hex >> byte)) return std::nullopt;
        result.push_back(static_cast<uint8_t>(byte));
    }
    return result;
}

} // namespace util
} // namespace chrono
