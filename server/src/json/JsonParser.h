/**
 * Chrono-shift C++ JSON 解析器
 * 递归下降解析 JSON 字符串到 JsonValue 树
 * C++17 重构版
 */
#ifndef CHRONO_CPP_JSON_PARSER_H
#define CHRONO_CPP_JSON_PARSER_H

#include "JsonValue.h"
#include <string>
#include <memory>
#include <optional>

namespace chrono {
namespace json {

/**
 * JSON 解析器类
 * 解析 JSON 字符串 → JsonValue 树
 * 安全限制: 最大深度 64, 最大字符串 1MB, 最大元素 256K
 */
class JsonParser {
public:
    JsonParser() = default;

    /**
     * 解析 JSON 字符串
     * @param input 以 null 结尾的 JSON 字符串
     * @return 解析成功返回 JsonValue，失败返回 std::nullopt
     */
    std::optional<JsonValue> parse(const std::string& input);
    std::optional<JsonValue> parse(const char* input);

    /**
     * 获取最后一个错误信息
     */
    const std::string& last_error() const { return error_; }

    /**
     * 便利函数: 从 JSON 字符串中直接提取字符串字段
     */
    static std::optional<std::string> extract_string(const std::string& json_str, const std::string& key);

    /**
     * 便利函数: 从 JSON 字符串中直接提取数值字段
     */
    static std::optional<double> extract_number(const std::string& json_str, const std::string& key);

private:
    // 解析器内部状态
    std::string input_;
    size_t pos_ = 0;
    size_t length_ = 0;
    unsigned int depth_ = 0;
    std::string error_;

    // 安全限制常量
    static constexpr unsigned int kMaxDepth = 64;
    static constexpr size_t kMaxStringLength = 1024 * 1024;  // 1MB
    static constexpr size_t kMaxElementCount = 256 * 1024;    // 256K

    // 词法分析
    void skip_whitespace();
    int peek() const;
    int advance();
    bool match(char expected);

    // 解析函数
    std::optional<JsonValue> parse_value();
    std::optional<JsonValue> parse_object();
    std::optional<JsonValue> parse_array();
    std::optional<JsonValue> parse_string();
    std::optional<JsonValue> parse_number();
    std::optional<JsonValue> parse_bool();
    std::optional<JsonValue> parse_null();
};

} // namespace json
} // namespace chrono

#endif // CHRONO_CPP_JSON_PARSER_H
