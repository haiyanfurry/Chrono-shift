/**
 * Chrono-shift C++ JSON 解析器实现
 * 递归下降解析器
 * C++17 重构版
 */
#include "JsonParser.h"
#include <cstring>
#include <cstdlib>
#include <cctype>
#include <cmath>

namespace chrono {
namespace json {

// ============================================================
// 主解析入口
// ============================================================

std::optional<JsonValue> JsonParser::parse(const std::string& input) {
    input_ = input;
    pos_ = 0;
    length_ = input_.length();
    depth_ = 0;
    error_.clear();

    skip_whitespace();
    auto val = parse_value();
    if (val) {
        skip_whitespace(); // 允许尾随空白
    }
    return val;
}

std::optional<JsonValue> JsonParser::parse(const char* input) {
    if (!input) {
        error_ = "null input";
        return std::nullopt;
    }
    return parse(std::string(input));
}

// ============================================================
// 词法分析
// ============================================================

void JsonParser::skip_whitespace() {
    while (pos_ < length_) {
        char c = input_[pos_];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            pos_++;
        } else {
            break;
        }
    }
}

int JsonParser::peek() const {
    if (pos_ >= length_) return -1;
    return static_cast<unsigned char>(input_[pos_]);
}

int JsonParser::advance() {
    if (pos_ >= length_) return -1;
    return static_cast<unsigned char>(input_[pos_++]);
}

bool JsonParser::match(char expected) {
    if (peek() == expected) {
        advance();
        return true;
    }
    return false;
}

// ============================================================
// 值解析分发
// ============================================================

std::optional<JsonValue> JsonParser::parse_value() {
    skip_whitespace();
    int c = peek();
    if (c < 0) return std::nullopt;

    // 递归深度限制
    if (depth_ >= kMaxDepth) {
        error_ = "max depth exceeded";
        return std::nullopt;
    }

    switch (c) {
        case '{': {
            depth_++;
            auto obj = parse_object();
            depth_--;
            return obj;
        }
        case '[': {
            depth_++;
            auto arr = parse_array();
            depth_--;
            return arr;
        }
        case '"': return parse_string();
        case 't': case 'f': return parse_bool();
        case 'n': return parse_null();
        default:
            if (c == '-' || (c >= '0' && c <= '9')) {
                return parse_number();
            }
            error_ = "unexpected character";
            return std::nullopt;
    }
}

// ============================================================
// 对象解析
// ============================================================

std::optional<JsonValue> JsonParser::parse_object() {
    advance(); // 吃掉 '{'

    std::map<std::string, JsonValue> obj;

    skip_whitespace();
    if (match('}')) {
        return JsonValue(std::move(obj));
    }

    while (true) {
        skip_whitespace();

        // key 必须是字符串
        if (peek() != '"') {
            error_ = "expected string key";
            return std::nullopt;
        }
        auto key_val = parse_string();
        if (!key_val) return std::nullopt;

        std::string key = key_val->get_string();

        skip_whitespace();
        if (!match(':')) {
            error_ = "expected ':'";
            return std::nullopt;
        }

        skip_whitespace();
        auto value = parse_value();
        if (!value) return std::nullopt;

        // 最大元素数量限制
        if (obj.size() >= kMaxElementCount) {
            error_ = "too many elements";
            return std::nullopt;
        }

        obj[std::move(key)] = std::move(*value);

        skip_whitespace();
        if (match(',')) {
            continue;
        } else if (match('}')) {
            break;
        } else {
            error_ = "expected ',' or '}'";
            return std::nullopt;
        }
    }

    return JsonValue(std::move(obj));
}

// ============================================================
// 数组解析
// ============================================================

std::optional<JsonValue> JsonParser::parse_array() {
    advance(); // 吃掉 '['

    std::vector<JsonValue> arr;

    skip_whitespace();
    if (match(']')) {
        return JsonValue(std::move(arr));
    }

    while (true) {
        auto val = parse_value();
        if (!val) return std::nullopt;

        // 最大元素数量限制
        if (arr.size() >= kMaxElementCount) {
            error_ = "too many elements";
            return std::nullopt;
        }

        arr.push_back(std::move(*val));

        skip_whitespace();
        if (match(',')) {
            continue;
        } else if (match(']')) {
            break;
        } else {
            error_ = "expected ',' or ']'";
            return std::nullopt;
        }
    }

    return JsonValue(std::move(arr));
}

// ============================================================
// 字符串解析
// ============================================================

std::optional<JsonValue> JsonParser::parse_string() {
    advance(); // 吃掉 '"'

    std::string str;
    str.reserve(64);

    while (pos_ < length_) {
        // 字符串最大长度检查
        if (str.length() >= kMaxStringLength) {
            error_ = "string too long";
            return std::nullopt;
        }

        int c = advance();
        if (c == '"') {
            return JsonValue(std::move(str));
        }

        if (c == '\\') {
            if (pos_ >= length_) {
                error_ = "unterminated escape";
                return std::nullopt;
            }
            int esc = advance();
            switch (esc) {
                case '"':  c = '"';  break;
                case '\\': c = '\\'; break;
                case '/':  c = '/';  break;
                case 'b':  c = '\b'; break;
                case 'f':  c = '\f'; break;
                case 'n':  c = '\n'; break;
                case 'r':  c = '\r'; break;
                case 't':  c = '\t'; break;
                case 'u': {
                    // Unicode escape (仅支持 BMP)
                    if (pos_ + 4 > length_) {
                        error_ = "invalid unicode escape";
                        return std::nullopt;
                    }
                    unsigned int codepoint = 0;
                    for (int i = 0; i < 4; i++) {
                        int hex = advance();
                        if (hex >= '0' && hex <= '9')
                            codepoint = (codepoint << 4) | (hex - '0');
                        else if (hex >= 'a' && hex <= 'f')
                            codepoint = (codepoint << 4) | (hex - 'a' + 10);
                        else if (hex >= 'A' && hex <= 'F')
                            codepoint = (codepoint << 4) | (hex - 'A' + 10);
                        else {
                            error_ = "invalid hex digit";
                            return std::nullopt;
                        }
                    }
                    // 仅支持 ASCII 范围
                    if (codepoint < 128) {
                        c = static_cast<int>(codepoint);
                    } else {
                        c = '?'; // 替换非 ASCII
                    }
                    break;
                }
                default:
                    error_ = "invalid escape character";
                    return std::nullopt;
            }
        }

        str += static_cast<char>(c);
    }

    error_ = "unterminated string";
    return std::nullopt;
}

// ============================================================
// 数字解析
// ============================================================

std::optional<JsonValue> JsonParser::parse_number() {
    size_t start = pos_;

    // 可选的负号
    if (peek() == '-') advance();

    // 整数部分
    if (peek() == '0') {
        advance();
    } else if (peek() >= '1' && peek() <= '9') {
        advance();
        while (pos_ < length_ && peek() >= '0' && peek() <= '9') {
            advance();
        }
    } else {
        error_ = "invalid number";
        return std::nullopt;
    }

    // 可选的小数部分
    bool is_double = false;
    if (peek() == '.') {
        is_double = true;
        advance();
        if (!(peek() >= '0' && peek() <= '9')) {
            error_ = "invalid decimal";
            return std::nullopt;
        }
        while (pos_ < length_ && peek() >= '0' && peek() <= '9') {
            advance();
        }
    }

    // 可选的指数部分
    if (peek() == 'e' || peek() == 'E') {
        is_double = true;
        advance();
        if (peek() == '+' || peek() == '-') advance();
        if (!(peek() >= '0' && peek() <= '9')) {
            error_ = "invalid exponent";
            return std::nullopt;
        }
        while (pos_ < length_ && peek() >= '0' && peek() <= '9') {
            advance();
        }
    }

    // 提取数字字符串
    size_t end = pos_;
    std::string num_str = input_.substr(start, end - start);

    if (is_double) {
        char* end_ptr = nullptr;
        double val = std::strtod(num_str.c_str(), &end_ptr);
        if (end_ptr != num_str.c_str() + num_str.length()) {
            error_ = "invalid double";
            return std::nullopt;
        }
        return JsonValue(val);
    } else {
        char* end_ptr = nullptr;
        int64_t val = static_cast<int64_t>(std::strtoll(num_str.c_str(), &end_ptr, 10));
        if (end_ptr != num_str.c_str() + num_str.length()) {
            error_ = "invalid integer";
            return std::nullopt;
        }
        return JsonValue(val);
    }
}

// ============================================================
// 布尔和 null 解析
// ============================================================

std::optional<JsonValue> JsonParser::parse_bool() {
    if (pos_ + 4 <= length_ &&
        input_.compare(pos_, 4, "true") == 0) {
        pos_ += 4;
        return JsonValue(true);
    }
    if (pos_ + 5 <= length_ &&
        input_.compare(pos_, 5, "false") == 0) {
        pos_ += 5;
        return JsonValue(false);
    }
    error_ = "invalid boolean";
    return std::nullopt;
}

std::optional<JsonValue> JsonParser::parse_null() {
    if (pos_ + 4 <= length_ &&
        input_.compare(pos_, 4, "null") == 0) {
        pos_ += 4;
        return JsonValue(JsonNull{});
    }
    error_ = "invalid null";
    return std::nullopt;
}

// ============================================================
// 便利函数
// ============================================================

std::optional<std::string> JsonParser::extract_string(
    const std::string& json_str, const std::string& key) {
    JsonParser parser;
    auto root = parser.parse(json_str);
    if (!root) return std::nullopt;
    if (!root->is_object()) return std::nullopt;
    if (!root->has_key(key)) return std::nullopt;
    const auto& val = (*root)[key];
    if (!val.is_string()) return std::nullopt;
    return val.as_string();
}

std::optional<double> JsonParser::extract_number(
    const std::string& json_str, const std::string& key) {
    JsonParser parser;
    auto root = parser.parse(json_str);
    if (!root) return std::nullopt;
    if (!root->is_object()) return std::nullopt;
    if (!root->has_key(key)) return std::nullopt;
    const auto& val = (*root)[key];
    if (!val.is_number()) return std::nullopt;
    return val.as_double();
}

} // namespace json
} // namespace chrono
