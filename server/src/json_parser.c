/**
 * Chrono-shift 简单 JSON 解析器
 * 语言标准: C99
 *
 * 轻量级 JSON 解析，不依赖第三方库
 * 递归下降解析器，支持完整的 JSON 语法
 * 构建 API: 生成 JSON 字符串
 * 解析 API: 解析 JSON 到树形结构
 */

#include "server.h"
#include "json_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <math.h>

/* ============================================================
 * 内部解析器状态
 * ============================================================ */

typedef struct {
    const char* input;
    size_t pos;
    size_t length;
    char error[256];
} JsonParser;

/* --- 前向声明 --- */
static JsonValue* json_parse_value(JsonParser* parser);
static JsonValue* json_parse_object(JsonParser* parser);
static JsonValue* json_parse_array(JsonParser* parser);
static JsonValue* json_parse_string(JsonParser* parser);
static JsonValue* json_parse_number(JsonParser* parser);
static JsonValue* json_parse_bool(JsonParser* parser);
static JsonValue* json_parse_null(JsonParser* parser);
static void json_skip_whitespace(JsonParser* parser);

/* ============================================================
 * JSON 值创建/释放
 * ============================================================ */

static JsonValue* json_value_create(JsonType type)
{
    JsonValue* val = (JsonValue*)calloc(1, sizeof(JsonValue));
    if (val) val->type = type;
    return val;
}

void json_value_free(JsonValue* val)
{
    if (!val) return;

    switch (val->type) {
        case JSON_STRING:
            free(val->string_val);
            break;
        case JSON_ARRAY:
            for (size_t i = 0; i < val->array.count; i++) {
                json_value_free(val->array.items[i]);
            }
            free(val->array.items);
            break;
        case JSON_OBJECT:
            for (size_t i = 0; i < val->object.count; i++) {
                free(val->object.pairs[i].key);
                json_value_free(val->object.pairs[i].value);
            }
            free(val->object.pairs);
            break;
        default:
            break;
    }
    free(val);
}

/* ============================================================
 * 词法分析辅助
 * ============================================================ */

static void json_skip_whitespace(JsonParser* parser)
{
    while (parser->pos < parser->length) {
        char c = parser->input[parser->pos];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            parser->pos++;
        } else {
            break;
        }
    }
}

static int json_peek(JsonParser* parser)
{
    if (parser->pos >= parser->length) return -1;
    return (unsigned char)parser->input[parser->pos];
}

static int json_advance(JsonParser* parser)
{
    if (parser->pos >= parser->length) return -1;
    return (unsigned char)parser->input[parser->pos++];
}

static int json_match(JsonParser* parser, char expected)
{
    if (json_peek(parser) == expected) {
        json_advance(parser);
        return 1;
    }
    return 0;
}

/* ============================================================
 * 递归下降解析
 * ============================================================ */

/* 解析入口 */
JsonValue* json_parse(const char* input)
{
    if (!input) return NULL;

    JsonParser parser;
    parser.input = input;
    parser.pos = 0;
    parser.length = strlen(input);
    parser.error[0] = '\0';

    json_skip_whitespace(&parser);
    JsonValue* val = json_parse_value(&parser);
    if (val) {
        json_skip_whitespace(&parser);
        /* 检查是否有尾随字符（允许） */
    }
    return val;
}

static JsonValue* json_parse_value(JsonParser* parser)
{
    json_skip_whitespace(parser);
    int c = json_peek(parser);
    if (c < 0) return NULL;

    switch (c) {
        case '{': return json_parse_object(parser);
        case '[': return json_parse_array(parser);
        case '"': return json_parse_string(parser);
        case 't': case 'f': return json_parse_bool(parser);
        case 'n': return json_parse_null(parser);
        default:
            if (c == '-' || (c >= '0' && c <= '9')) {
                return json_parse_number(parser);
            }
            return NULL;
    }
}

static JsonValue* json_parse_object(JsonParser* parser)
{
    json_advance(parser); /* 吃掉 '{' */

    JsonValue* obj = json_value_create(JSON_OBJECT);
    if (!obj) return NULL;

    /* 分配初始容量 */
    size_t capacity = 8;
    obj->object.pairs = (JsonPair*)malloc(capacity * sizeof(JsonPair));
    if (!obj->object.pairs) {
        free(obj);
        return NULL;
    }
    obj->object.count = 0;

    json_skip_whitespace(parser);

    /* 空对象 */
    if (json_match(parser, '}')) {
        return obj;
    }

    while (1) {
        json_skip_whitespace(parser);

        /* 解析 key (必须是字符串) */
        if (json_peek(parser) != '"') {
            json_value_free(obj);
            return NULL;
        }
        JsonValue* key_val = json_parse_string(parser);
        if (!key_val) {
            json_value_free(obj);
            return NULL;
        }

        json_skip_whitespace(parser);
        if (!json_match(parser, ':')) {
            json_value_free(key_val);
            json_value_free(obj);
            return NULL;
        }

        json_skip_whitespace(parser);
        JsonValue* value = json_parse_value(parser);
        if (!value) {
            json_value_free(key_val);
            json_value_free(obj);
            return NULL;
        }

        /* 扩展容量 */
        if (obj->object.count >= capacity) {
            capacity *= 2;
            JsonPair* new_pairs = (JsonPair*)realloc(obj->object.pairs,
                                                      capacity * sizeof(JsonPair));
            if (!new_pairs) {
                json_value_free(key_val);
                json_value_free(value);
                json_value_free(obj);
                return NULL;
            }
            obj->object.pairs = new_pairs;
        }

        obj->object.pairs[obj->object.count].key = key_val->string_val;
        key_val->string_val = NULL; /* 转移所有权 */
        json_value_free(key_val);
        obj->object.pairs[obj->object.count].value = value;
        obj->object.count++;

        json_skip_whitespace(parser);
        if (json_match(parser, ',')) {
            continue;
        } else if (json_match(parser, '}')) {
            break;
        } else {
            json_value_free(obj);
            return NULL;
        }
    }

    return obj;
}

static JsonValue* json_parse_array(JsonParser* parser)
{
    json_advance(parser); /* 吃掉 '[' */

    JsonValue* arr = json_value_create(JSON_ARRAY);
    if (!arr) return NULL;

    size_t capacity = 8;
    arr->array.items = (JsonValue**)malloc(capacity * sizeof(JsonValue*));
    if (!arr->array.items) {
        free(arr);
        return NULL;
    }
    arr->array.count = 0;

    json_skip_whitespace(parser);

    /* 空数组 */
    if (json_match(parser, ']')) {
        return arr;
    }

    while (1) {
        JsonValue* val = json_parse_value(parser);
        if (!val) {
            json_value_free(arr);
            return NULL;
        }

        if (arr->array.count >= capacity) {
            capacity *= 2;
            JsonValue** new_items = (JsonValue**)realloc(arr->array.items,
                                                          capacity * sizeof(JsonValue*));
            if (!new_items) {
                json_value_free(val);
                json_value_free(arr);
                return NULL;
            }
            arr->array.items = new_items;
        }

        arr->array.items[arr->array.count++] = val;

        json_skip_whitespace(parser);
        if (json_match(parser, ',')) {
            continue;
        } else if (json_match(parser, ']')) {
            break;
        } else {
            json_value_free(arr);
            return NULL;
        }
    }

    return arr;
}

static JsonValue* json_parse_string(JsonParser* parser)
{
    json_advance(parser); /* 吃掉 '"' */

    /* 估算最大长度 */
    size_t capacity = 64;
    char* str = (char*)malloc(capacity);
    if (!str) return NULL;
    size_t len = 0;

    while (parser->pos < parser->length) {
        int c = json_advance(parser);
        if (c == '"') {
            str[len] = '\0';
            JsonValue* val = json_value_create(JSON_STRING);
            if (!val) {
                free(str);
                return NULL;
            }
            val->string_val = str;
            return val;
        }

        if (c == '\\') {
            if (parser->pos >= parser->length) {
                free(str);
                return NULL;
            }
            int esc = json_advance(parser);
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
                    /* 简单的 Unicode escape (仅支持 BMP, 不支持 surrogate pair) */
                    if (parser->pos + 4 > parser->length) {
                        free(str);
                        return NULL;
                    }
                    unsigned int codepoint = 0;
                    for (int i = 0; i < 4; i++) {
                        int hex = json_advance(parser);
                        if (hex >= '0' && hex <= '9')
                            codepoint = (codepoint << 4) | (hex - '0');
                        else if (hex >= 'a' && hex <= 'f')
                            codepoint = (codepoint << 4) | (hex - 'a' + 10);
                        else if (hex >= 'A' && hex <= 'F')
                            codepoint = (codepoint << 4) | (hex - 'A' + 10);
                        else {
                            free(str);
                            return NULL;
                        }
                    }
                    /* 仅支持 ASCII 范围 */
                    if (codepoint < 128) {
                        c = (int)codepoint;
                    } else {
                        c = '?'; /* 替换非 ASCII */
                    }
                    break;
                }
                default:
                    free(str);
                    return NULL;
            }
        }

        /* 扩展缓冲区 */
        if (len + 1 >= capacity) {
            capacity *= 2;
            char* new_str = (char*)realloc(str, capacity);
            if (!new_str) {
                free(str);
                return NULL;
            }
            str = new_str;
        }
        str[len++] = (char)c;
    }

    /* 未闭合的字符串 */
    free(str);
    return NULL;
}

static JsonValue* json_parse_number(JsonParser* parser)
{
    size_t start = parser->pos;

    /* 可选的负号 */
    if (json_peek(parser) == '-') json_advance(parser);

    /* 整数部分 */
    if (json_peek(parser) == '0') {
        json_advance(parser);
    } else if (json_peek(parser) >= '1' && json_peek(parser) <= '9') {
        json_advance(parser);
        while (parser->pos < parser->length &&
               json_peek(parser) >= '0' && json_peek(parser) <= '9') {
            json_advance(parser);
        }
    } else {
        return NULL;
    }

    /* 可选的小数部分 */
    int is_double = 0;
    if (json_peek(parser) == '.') {
        is_double = 1;
        json_advance(parser);
        if (!(json_peek(parser) >= '0' && json_peek(parser) <= '9')) {
            return NULL;
        }
        while (parser->pos < parser->length &&
               json_peek(parser) >= '0' && json_peek(parser) <= '9') {
            json_advance(parser);
        }
    }

    /* 可选的指数部分 */
    if (json_peek(parser) == 'e' || json_peek(parser) == 'E') {
        is_double = 1;
        json_advance(parser);
        if (json_peek(parser) == '+' || json_peek(parser) == '-') {
            json_advance(parser);
        }
        if (!(json_peek(parser) >= '0' && json_peek(parser) <= '9')) {
            return NULL;
        }
        while (parser->pos < parser->length &&
               json_peek(parser) >= '0' && json_peek(parser) <= '9') {
            json_advance(parser);
        }
    }

    /* 提取数字字符串 */
    size_t end = parser->pos;
    size_t len = end - start;
    char* num_str = (char*)malloc(len + 1);
    if (!num_str) return NULL;
    memcpy(num_str, parser->input + start, len);
    num_str[len] = '\0';

    JsonValue* val = json_value_create(JSON_NUMBER);
    if (!val) {
        free(num_str);
        return NULL;
    }
    val->number_val = strtod(num_str, NULL);
    free(num_str);
    return val;
}

static JsonValue* json_parse_bool(JsonParser* parser)
{
    if (parser->pos + 4 <= parser->length &&
        memcmp(parser->input + parser->pos, "true", 4) == 0) {
        parser->pos += 4;
        JsonValue* val = json_value_create(JSON_BOOL);
        if (val) val->bool_val = 1;
        return val;
    }
    if (parser->pos + 5 <= parser->length &&
        memcmp(parser->input + parser->pos, "false", 5) == 0) {
        parser->pos += 5;
        JsonValue* val = json_value_create(JSON_BOOL);
        if (val) val->bool_val = 0;
        return val;
    }
    return NULL;
}

static JsonValue* json_parse_null(JsonParser* parser)
{
    if (parser->pos + 4 <= parser->length &&
        memcmp(parser->input + parser->pos, "null", 4) == 0) {
        parser->pos += 4;
        return json_value_create(JSON_NULL);
    }
    return NULL;
}

/* ============================================================
 * 查询 API
 * ============================================================ */

JsonValue* json_object_get(const JsonValue* val, const char* key)
{
    if (!val || val->type != JSON_OBJECT || !key) return NULL;

    for (size_t i = 0; i < val->object.count; i++) {
        if (strcmp(val->object.pairs[i].key, key) == 0) {
            return val->object.pairs[i].value;
        }
    }
    return NULL;
}

JsonValue* json_array_get(const JsonValue* val, size_t index)
{
    if (!val || val->type != JSON_ARRAY || index >= val->array.count) {
        return NULL;
    }
    return val->array.items[index];
}

const char* json_as_string(const JsonValue* val)
{
    if (!val || val->type != JSON_STRING) return NULL;
    return val->string_val;
}

double json_as_number(const JsonValue* val)
{
    if (!val || val->type != JSON_NUMBER) return 0.0;
    return val->number_val;
}

int json_as_bool(const JsonValue* val)
{
    if (!val || val->type != JSON_BOOL) return 0;
    return val->bool_val;
}

int json_array_length(const JsonValue* val)
{
    if (!val || val->type != JSON_ARRAY) return -1;
    return (int)val->array.count;
}

/* ============================================================
 * 便利函数：从 JSON 字符串中直接提取字段
 * ============================================================ */

char* json_extract_string(const char* json_str, const char* key)
{
    if (!json_str || !key) return NULL;

    JsonValue* root = json_parse(json_str);
    if (!root) return NULL;

    JsonValue* field = json_object_get(root, key);
    char* result = NULL;
    if (field && field->type == JSON_STRING) {
        result = strdup(field->string_val);
    }

    json_value_free(root);
    return result;
}

double json_extract_number(const char* json_str, const char* key)
{
    if (!json_str || !key) return 0.0;

    JsonValue* root = json_parse(json_str);
    if (!root) return 0.0;

    JsonValue* field = json_object_get(root, key);
    double result = 0.0;
    if (field && field->type == JSON_NUMBER) {
        result = field->number_val;
    }

    json_value_free(root);
    return result;
}

/* ============================================================
 * 构建 API (保留原有功能)
 * ============================================================ */

char* json_build_response(const char* status, const char* message, const char* data_json)
{
    /* 构建 {"status":"ok","message":"...","data":{...}} */
    size_t len = strlen(status) + strlen(message) + (data_json ? strlen(data_json) : 0) + 128;
    char* result = (char*)malloc(len);
    if (result) {
        if (data_json) {
            snprintf(result, len, "{\"status\":\"%s\",\"message\":\"%s\",\"data\":%s}",
                     status, message, data_json);
        } else {
            snprintf(result, len, "{\"status\":\"%s\",\"message\":\"%s\"}",
                     status, message);
        }
    }
    return result;
}

char* json_build_error(const char* message)
{
    return json_build_response("error", message, NULL);
}

char* json_build_success(const char* data_json)
{
    return json_build_response("ok", "success", data_json);
}

char* json_escape_string(const char* input)
{
    if (!input) return NULL;

    size_t len = strlen(input);
    size_t escaped_len = len + 2; /* 引号 */
    for (size_t i = 0; i < len; i++) {
        if (input[i] == '"' || input[i] == '\\' || input[i] == '\n' || input[i] == '\t') {
            escaped_len++;
        }
    }

    char* result = (char*)malloc(escaped_len + 1);
    if (!result) return NULL;

    size_t j = 0;
    result[j++] = '"';
    for (size_t i = 0; i < len; i++) {
        switch (input[i]) {
            case '"':  result[j++] = '\\'; result[j++] = '"';  break;
            case '\\': result[j++] = '\\'; result[j++] = '\\'; break;
            case '\n': result[j++] = '\\'; result[j++] = 'n';  break;
            case '\t': result[j++] = '\\'; result[j++] = 't';  break;
            default:   result[j++] = input[i];                  break;
        }
    }
    result[j++] = '"';
    result[j] = '\0';

    return result;
}
