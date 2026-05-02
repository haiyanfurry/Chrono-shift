#ifndef CHRONO_JSON_PARSER_H
#define CHRONO_JSON_PARSER_H

#include <stdint.h>
#include <stddef.h>

/* ============================================================
 * JSON 解析器接口
 * 轻量级 JSON 构建/解析，不依赖第三方库
 *
 * 构建 API: 生成 JSON 字符串
 * 解析 API: 递归下降解析 JSON 到树形结构
 * ============================================================ */

/* --- JSON 值类型 --- */
typedef enum {
    JSON_NULL,
    JSON_BOOL,
    JSON_NUMBER,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT
} JsonType;

/* 前向声明 */
typedef struct JsonValue JsonValue;
typedef struct JsonPair JsonPair;

struct JsonValue {
    JsonType type;
    union {
        int bool_val;
        double number_val;
        char* string_val;
        struct {
            JsonValue** items;
            size_t count;
        } array;
        struct {
            JsonPair* pairs;
            size_t count;
        } object;
    };
};

struct JsonPair {
    char* key;
    JsonValue* value;
};

/* --- 构建 API --- */
char* json_build_response(const char* status, const char* message, const char* data_json);
char* json_build_error(const char* message);
char* json_build_success(const char* data_json);
char* json_escape_string(const char* input);

/* --- 解析 API --- */

/* 解析 JSON 字符串，返回根节点。失败返回 NULL */
JsonValue* json_parse(const char* input);

/* 释放整个 JSON 树 */
void json_value_free(JsonValue* val);

/* --- 查询 API --- */

/* 通过 key 获取 object 中的子值。val 必须是 JSON_OBJECT 类型 */
JsonValue* json_object_get(const JsonValue* val, const char* key);

/* 通过 index 获取 array 中的子值。val 必须是 JSON_ARRAY 类型 */
JsonValue* json_array_get(const JsonValue* val, size_t index);

/* 获取字符串值（返回内部指针，不要 free）。val 必须是 JSON_STRING 类型 */
const char* json_as_string(const JsonValue* val);

/* 获取数值。val 必须是 JSON_NUMBER 类型 */
double json_as_number(const JsonValue* val);

/* 获取布尔值。val 必须是 JSON_BOOL 类型 */
int json_as_bool(const JsonValue* val);

/* 获取数组长度。val 必须是 JSON_ARRAY 类型，返回 -1 如果类型不匹配 */
int json_array_length(const JsonValue* val);

/* 便利函数：从 JSON 字符串中直接提取字符串字段 */
char* json_extract_string(const char* json_str, const char* key);

/* 便利函数：从 JSON 字符串中直接提取数值字段 */
double json_extract_number(const char* json_str, const char* key);

#endif /* CHRONO_JSON_PARSER_H */
