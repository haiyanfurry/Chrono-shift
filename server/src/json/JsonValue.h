/**
 * Chrono-shift C++ JSON 值类型
 * 使用 std::variant 实现类型安全的 JSON 节点
 * C++17 重构版
 */
#ifndef CHRONO_CPP_JSON_VALUE_H
#define CHRONO_CPP_JSON_VALUE_H

#include <variant>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cstdint>
#include <stdexcept>
#include <ostream>

namespace chrono {
namespace json {

// 前向声明
class JsonValue;

// JSON null 类型
struct JsonNull {};

// 比较运算符
inline bool operator==(const JsonNull&, const JsonNull&) { return true; }
inline bool operator!=(const JsonNull&, const JsonNull&) { return false; }

// 内部值变体类型
using JsonVariant = std::variant<
    JsonNull,           // null
    bool,               // true/false
    int64_t,            // 整数
    double,             // 浮点数
    std::string,        // 字符串
    std::vector<JsonValue>,  // 数组
    std::map<std::string, JsonValue> // 对象
>;

/**
 * JSON 值类 — RAII 包装 std::variant
 * 自动内存管理，无需手动 free
 */
class JsonValue {
public:
    // 构造函数
    JsonValue() : value_(JsonNull{}) {}
    JsonValue(JsonNull) : value_(JsonNull{}) {}
    JsonValue(bool b) : value_(b) {}
    JsonValue(int32_t i) : value_(static_cast<int64_t>(i)) {}
    JsonValue(int64_t i) : value_(i) {}
    JsonValue(double d) : value_(d) {}
    JsonValue(const char* s) : value_(std::string(s)) {}
    JsonValue(const std::string& s) : value_(s) {}
    JsonValue(std::string&& s) noexcept : value_(std::move(s)) {}
    JsonValue(const std::vector<JsonValue>& arr) : value_(arr) {}
    JsonValue(std::vector<JsonValue>&& arr) noexcept : value_(std::move(arr)) {}
    JsonValue(const std::map<std::string, JsonValue>& obj) : value_(obj) {}
    JsonValue(std::map<std::string, JsonValue>&& obj) noexcept : value_(std::move(obj)) {}

    // 类型判断
    bool is_null()   const { return std::holds_alternative<JsonNull>(value_); }
    bool is_bool()   const { return std::holds_alternative<bool>(value_); }
    bool is_int()    const { return std::holds_alternative<int64_t>(value_); }
    bool is_double() const { return std::holds_alternative<double>(value_); }
    bool is_number() const { return is_int() || is_double(); }
    bool is_string() const { return std::holds_alternative<std::string>(value_); }
    bool is_array()  const { return std::holds_alternative<std::vector<JsonValue>>(value_); }
    bool is_object() const { return std::holds_alternative<std::map<std::string, JsonValue>>(value_); }

    // 值获取 (抛出异常如果类型不匹配)
    bool        as_bool()   const { return std::get<bool>(value_); }
    int64_t     as_int()    const { return std::get<int64_t>(value_); }
    double      as_double() const;
    double      as_number() const { return as_double(); }
    const std::string& as_string() const { return std::get<std::string>(value_); }

    // 安全获取 (返回默认值如果类型不匹配)
    bool        get_bool(bool def = false)   const;
    int64_t     get_int(int64_t def = 0)     const;
    double      get_double(double def = 0.0) const;
    std::string get_string(const std::string& def = "") const;

    // 数组操作
    size_t array_size() const;
    bool   array_empty() const;
    const JsonValue& operator[](size_t index) const;
    JsonValue& operator[](size_t index);
    void   array_push_back(const JsonValue& val);
    void   array_push_back(JsonValue&& val);

    // 对象操作
    size_t object_size() const;
    bool   object_empty() const;
    bool   has_key(const std::string& key) const;
    const JsonValue& operator[](const std::string& key) const;
    JsonValue& operator[](const std::string& key);
    void   object_insert(const std::string& key, const JsonValue& val);
    void   object_insert(const std::string& key, JsonValue&& val);

    // 迭代器支持
    using ArrayIter = std::vector<JsonValue>::const_iterator;
    using ObjectIter = std::map<std::string, JsonValue>::const_iterator;

    ArrayIter array_begin() const;
    ArrayIter array_end() const;
    ObjectIter object_begin() const;
    ObjectIter object_end() const;

    // 序列化为 JSON 字符串
    std::string serialize(bool pretty = false, int indent_level = 0) const;

    // 调试输出
    friend std::ostream& operator<<(std::ostream& os, const JsonValue& val) {
        os << val.serialize();
        return os;
    }

    // 比较
    bool operator==(const JsonValue& other) const { return value_ == other.value_; }
    bool operator!=(const JsonValue& other) const { return value_ != other.value_; }

private:
    JsonVariant value_;

    static std::string escape_string(const std::string& s);
    static std::string indent(int level);
};

// 便利函数
inline JsonValue json_null() { return JsonValue(JsonNull{}); }
inline JsonValue json_bool(bool b) { return JsonValue(b); }
inline JsonValue json_int(int64_t i) { return JsonValue(i); }
inline JsonValue json_double(double d) { return JsonValue(d); }
inline JsonValue json_string(const std::string& s) { return JsonValue(s); }
inline JsonValue json_array() { return JsonValue(std::vector<JsonValue>{}); }
inline JsonValue json_object() { return JsonValue(std::map<std::string, JsonValue>{}); }

// 构建 API (兼容旧 C API 命名)
JsonValue build_response(const std::string& status, const std::string& message);
JsonValue build_response(const std::string& status, const std::string& message, const JsonValue& data);
JsonValue build_error(const std::string& message);
JsonValue build_success(const JsonValue& data);

} // namespace json
} // namespace chrono

#endif // CHRONO_CPP_JSON_VALUE_H
