/**
 * Chrono-shift C++ JSON 值类型实现
 * 序列化、查询、构建 API
 * C++17 重构版
 */
#include "JsonValue.h"
#include <sstream>
#include <iomanip>
#include <cmath>
#include <cstring>

namespace chrono {
namespace json {

// ============================================================
// 安全获取 (返回默认值)
// ============================================================

bool JsonValue::get_bool(bool def) const {
    if (is_bool()) return std::get<bool>(value_);
    return def;
}

int64_t JsonValue::get_int(int64_t def) const {
    if (is_int()) return std::get<int64_t>(value_);
    return def;
}

double JsonValue::get_double(double def) const {
    if (is_double()) return std::get<double>(value_);
    if (is_int()) return static_cast<double>(std::get<int64_t>(value_));
    return def;
}

double JsonValue::as_double() const {
    if (is_double()) return std::get<double>(value_);
    if (is_int()) return static_cast<double>(std::get<int64_t>(value_));
    throw std::bad_variant_access();
}

std::string JsonValue::get_string(const std::string& def) const {
    if (is_string()) return std::get<std::string>(value_);
    return def;
}

// ============================================================
// 数组操作
// ============================================================

size_t JsonValue::array_size() const {
    if (is_array()) return std::get<std::vector<JsonValue>>(value_).size();
    return 0;
}

bool JsonValue::array_empty() const {
    if (is_array()) return std::get<std::vector<JsonValue>>(value_).empty();
    return true;
}

const JsonValue& JsonValue::operator[](size_t index) const {
    return std::get<std::vector<JsonValue>>(value_).at(index);
}

JsonValue& JsonValue::operator[](size_t index) {
    return std::get<std::vector<JsonValue>>(value_).at(index);
}

void JsonValue::array_push_back(const JsonValue& val) {
    std::get<std::vector<JsonValue>>(value_).push_back(val);
}

void JsonValue::array_push_back(JsonValue&& val) {
    std::get<std::vector<JsonValue>>(value_).push_back(std::move(val));
}

JsonValue::ArrayIter JsonValue::array_begin() const {
    return std::get<std::vector<JsonValue>>(value_).begin();
}

JsonValue::ArrayIter JsonValue::array_end() const {
    return std::get<std::vector<JsonValue>>(value_).end();
}

// ============================================================
// 对象操作
// ============================================================

size_t JsonValue::object_size() const {
    if (is_object()) return std::get<std::map<std::string, JsonValue>>(value_).size();
    return 0;
}

bool JsonValue::object_empty() const {
    if (is_object()) return std::get<std::map<std::string, JsonValue>>(value_).empty();
    return true;
}

bool JsonValue::has_key(const std::string& key) const {
    if (!is_object()) return false;
    const auto& map = std::get<std::map<std::string, JsonValue>>(value_);
    return map.find(key) != map.end();
}

const JsonValue& JsonValue::operator[](const std::string& key) const {
    const auto& map = std::get<std::map<std::string, JsonValue>>(value_);
    auto it = map.find(key);
    if (it == map.end()) {
        throw std::out_of_range("JSON key not found: " + key);
    }
    return it->second;
}

JsonValue& JsonValue::operator[](const std::string& key) {
    auto& map = std::get<std::map<std::string, JsonValue>>(value_);
    return map[key];
}

void JsonValue::object_insert(const std::string& key, const JsonValue& val) {
    std::get<std::map<std::string, JsonValue>>(value_)[key] = val;
}

void JsonValue::object_insert(const std::string& key, JsonValue&& val) {
    std::get<std::map<std::string, JsonValue>>(value_)[key] = std::move(val);
}

JsonValue::ObjectIter JsonValue::object_begin() const {
    return std::get<std::map<std::string, JsonValue>>(value_).begin();
}

JsonValue::ObjectIter JsonValue::object_end() const {
    return std::get<std::map<std::string, JsonValue>>(value_).end();
}

// ============================================================
// 序列化
// ============================================================

std::string JsonValue::escape_string(const std::string& s) {
    std::string result;
    result.reserve(s.length() + 2);  // 预分配
    for (unsigned char c : s) {
        switch (c) {
            case '"':  result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\b': result += "\\b";  break;
            case '\f': result += "\\f";  break;
            case '\n': result += "\\n";  break;
            case '\r': result += "\\r";  break;
            case '\t': result += "\\t";  break;
            default:
                if (c < 0x20) {
                    // 控制字符: \uXXXX
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    result += buf;
                } else {
                    result += static_cast<char>(c);
                }
                break;
        }
    }
    return result;
}

std::string JsonValue::indent(int level) {
    return std::string(static_cast<size_t>(level) * 2, ' ');
}

std::string JsonValue::serialize(bool pretty, int indent_level) const {
    std::string result;

    if (is_null()) {
        result = "null";
    } else if (is_bool()) {
        result = std::get<bool>(value_) ? "true" : "false";
    } else if (is_int()) {
        result = std::to_string(std::get<int64_t>(value_));
    } else if (is_double()) {
        double d = std::get<double>(value_);
        // 检查是否为整数
        if (d == std::floor(d) && std::isfinite(d)) {
            result = std::to_string(static_cast<int64_t>(d));
        } else {
            std::ostringstream oss;
            oss << std::setprecision(17) << d;
            result = oss.str();
            // 确保包含小数点
            if (result.find('.') == std::string::npos &&
                result.find('e') == std::string::npos &&
                result.find('E') == std::string::npos) {
                result += ".0";
            }
        }
    } else if (is_string()) {
        result = "\"" + escape_string(std::get<std::string>(value_)) + "\"";
    } else if (is_array()) {
        const auto& arr = std::get<std::vector<JsonValue>>(value_);
        if (arr.empty()) {
            result = "[]";
        } else if (pretty) {
            result = "[\n";
            for (size_t i = 0; i < arr.size(); i++) {
                if (i > 0) result += ",\n";
                result += indent(indent_level + 1);
                result += arr[i].serialize(true, indent_level + 1);
            }
            result += "\n" + indent(indent_level) + "]";
        } else {
            result = "[";
            for (size_t i = 0; i < arr.size(); i++) {
                if (i > 0) result += ",";
                result += arr[i].serialize(false, 0);
            }
            result += "]";
        }
    } else if (is_object()) {
        const auto& obj = std::get<std::map<std::string, JsonValue>>(value_);
        if (obj.empty()) {
            result = "{}";
        } else if (pretty) {
            result = "{\n";
            bool first = true;
            for (const auto& [key, val] : obj) {
                if (!first) result += ",\n";
                first = false;
                result += indent(indent_level + 1);
                result += "\"" + escape_string(key) + "\": ";
                result += val.serialize(true, indent_level + 1);
            }
            result += "\n" + indent(indent_level) + "}";
        } else {
            result = "{";
            bool first = true;
            for (const auto& [key, val] : obj) {
                if (!first) result += ",";
                first = false;
                result += "\"" + escape_string(key) + "\":";
                result += val.serialize(false, 0);
            }
            result += "}";
        }
    }

    return result;
}

// ============================================================
// 构建 API (兼容旧 API 命名)
// ============================================================

JsonValue build_response(const std::string& status, const std::string& message) {
    JsonValue obj = json_object();
    obj["status"] = JsonValue(status);
    obj["message"] = JsonValue(message);
    return obj;
}

JsonValue build_response(const std::string& status, const std::string& message, const JsonValue& data) {
    JsonValue obj = json_object();
    obj["status"] = JsonValue(status);
    obj["message"] = JsonValue(message);
    obj["data"] = data;
    return obj;
}

JsonValue build_error(const std::string& message) {
    return build_response("error", message);
}

JsonValue build_success(const JsonValue& data) {
    return build_response("ok", "success", data);
}

} // namespace json
} // namespace chrono
