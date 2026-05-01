/**
 * test_json_parser.c — JSON 解析器单元测试
 *
 * 覆盖范围:
 * - 基本解析 (null, bool, number, string, array, object)
 * - 嵌套结构
 * - JSON 构建 (build_response, build_error, build_success)
 * - 字符串转义
 * - 边界/安全测试 (深度限制, 大字符串, 整数溢出)
 */
#include "unity.h"
#include "json_parser.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ============================================================
 * 辅助函数
 * ============================================================ */

static int g_global_setup_done = 0;

static void setup(void)
{
    if (!g_global_setup_done) {
        g_global_setup_done = 1;
    }
}

/* ============================================================
 * 测试: 解析 NULL
 * ============================================================ */
TEST_CASE(test_parse_null)
{
    JsonValue* v = json_parse("null");
    TEST_ASSERT_NOT_NULL(v);
    TEST_ASSERT_EQUAL_INT(JSON_NULL, v->type);
    json_value_free(v);
}

TEST_CASE(test_parse_null_whitespace)
{
    JsonValue* v = json_parse("  null  ");
    TEST_ASSERT_NOT_NULL(v);
    TEST_ASSERT_EQUAL_INT(JSON_NULL, v->type);
    json_value_free(v);
}

/* ============================================================
 * 测试: 解析 BOOL
 * ============================================================ */
TEST_CASE(test_parse_true)
{
    JsonValue* v = json_parse("true");
    TEST_ASSERT_NOT_NULL(v);
    TEST_ASSERT_EQUAL_INT(JSON_BOOL, v->type);
    TEST_ASSERT_EQUAL_INT(1, json_as_bool(v));
    json_value_free(v);
}

TEST_CASE(test_parse_false)
{
    JsonValue* v = json_parse("false");
    TEST_ASSERT_NOT_NULL(v);
    TEST_ASSERT_EQUAL_INT(JSON_BOOL, v->type);
    TEST_ASSERT_EQUAL_INT(0, json_as_bool(v));
    json_value_free(v);
}

/* ============================================================
 * 测试: 解析 NUMBER
 * ============================================================ */
TEST_CASE(test_parse_integer)
{
    JsonValue* v = json_parse("42");
    TEST_ASSERT_NOT_NULL(v);
    TEST_ASSERT_EQUAL_INT(JSON_NUMBER, v->type);
    TEST_ASSERT_EQUAL_DOUBLE(42.0, json_as_number(v), 0.001);
    json_value_free(v);
}

TEST_CASE(test_parse_negative)
{
    JsonValue* v = json_parse("-17");
    TEST_ASSERT_NOT_NULL(v);
    TEST_ASSERT_EQUAL_INT(JSON_NUMBER, v->type);
    TEST_ASSERT_EQUAL_DOUBLE(-17.0, json_as_number(v), 0.001);
    json_value_free(v);
}

TEST_CASE(test_parse_float)
{
    JsonValue* v = json_parse("3.14159");
    TEST_ASSERT_NOT_NULL(v);
    TEST_ASSERT_EQUAL_INT(JSON_NUMBER, v->type);
    TEST_ASSERT_EQUAL_DOUBLE(3.14159, json_as_number(v), 0.00001);
    json_value_free(v);
}

TEST_CASE(test_parse_scientific)
{
    JsonValue* v = json_parse("2.5e3");
    TEST_ASSERT_NOT_NULL(v);
    TEST_ASSERT_EQUAL_INT(JSON_NUMBER, v->type);
    TEST_ASSERT_EQUAL_DOUBLE(2500.0, json_as_number(v), 0.001);
    json_value_free(v);
}

TEST_CASE(test_parse_zero)
{
    JsonValue* v = json_parse("0");
    TEST_ASSERT_NOT_NULL(v);
    TEST_ASSERT_EQUAL_INT(JSON_NUMBER, v->type);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, json_as_number(v), 0.001);
    json_value_free(v);
}

/* ============================================================
 * 测试: 解析 STRING
 * ============================================================ */
TEST_CASE(test_parse_string_empty)
{
    JsonValue* v = json_parse("\"\"");
    TEST_ASSERT_NOT_NULL(v);
    TEST_ASSERT_EQUAL_INT(JSON_STRING, v->type);
    TEST_ASSERT_EQUAL_STRING("", json_as_string(v));
    json_value_free(v);
}

TEST_CASE(test_parse_string_hello)
{
    JsonValue* v = json_parse("\"hello\"");
    TEST_ASSERT_NOT_NULL(v);
    TEST_ASSERT_EQUAL_INT(JSON_STRING, v->type);
    TEST_ASSERT_EQUAL_STRING("hello", json_as_string(v));
    json_value_free(v);
}

TEST_CASE(test_parse_string_unicode)
{
    JsonValue* v = json_parse("\"\\u0048\\u0065\\u006C\\u006C\\u006F\"");
    TEST_ASSERT_NOT_NULL(v);
    TEST_ASSERT_EQUAL_INT(JSON_STRING, v->type);
    TEST_ASSERT_EQUAL_STRING("Hello", json_as_string(v));
    json_value_free(v);
}

TEST_CASE(test_parse_string_escaped)
{
    JsonValue* v = json_parse("\"line1\\nline2\\ttab\"");
    TEST_ASSERT_NOT_NULL(v);
    const char* s = json_as_string(v);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT(strchr(s, '\n') != NULL);
    TEST_ASSERT(strchr(s, '\t') != NULL);
    json_value_free(v);
}

/* ============================================================
 * 测试: 解析 ARRAY
 * ============================================================ */
TEST_CASE(test_parse_array_empty)
{
    JsonValue* v = json_parse("[]");
    TEST_ASSERT_NOT_NULL(v);
    TEST_ASSERT_EQUAL_INT(JSON_ARRAY, v->type);
    TEST_ASSERT_EQUAL_INT(0, json_array_length(v));
    json_value_free(v);
}

TEST_CASE(test_parse_array_numbers)
{
    JsonValue* v = json_parse("[1,2,3]");
    TEST_ASSERT_NOT_NULL(v);
    TEST_ASSERT_EQUAL_INT(JSON_ARRAY, v->type);
    TEST_ASSERT_EQUAL_INT(3, json_array_length(v));
    TEST_ASSERT_EQUAL_DOUBLE(1.0, json_as_number(json_array_get(v, 0)), 0.001);
    TEST_ASSERT_EQUAL_DOUBLE(2.0, json_as_number(json_array_get(v, 1)), 0.001);
    TEST_ASSERT_EQUAL_DOUBLE(3.0, json_as_number(json_array_get(v, 2)), 0.001);
    json_value_free(v);
}

TEST_CASE(test_parse_array_mixed)
{
    JsonValue* v = json_parse("[1, \"two\", false, null]");
    TEST_ASSERT_NOT_NULL(v);
    TEST_ASSERT_EQUAL_INT(JSON_ARRAY, v->type);
    TEST_ASSERT_EQUAL_INT(4, json_array_length(v));
    TEST_ASSERT_EQUAL_DOUBLE(1.0, json_as_number(json_array_get(v, 0)), 0.001);
    TEST_ASSERT_EQUAL_STRING("two", json_as_string(json_array_get(v, 1)));
    TEST_ASSERT_EQUAL_INT(0, json_as_bool(json_array_get(v, 2)));
    TEST_ASSERT_EQUAL_INT(JSON_NULL, json_array_get(v, 3)->type);
    json_value_free(v);
}

/* ============================================================
 * 测试: 解析 OBJECT
 * ============================================================ */
TEST_CASE(test_parse_object_empty)
{
    JsonValue* v = json_parse("{}");
    TEST_ASSERT_NOT_NULL(v);
    TEST_ASSERT_EQUAL_INT(JSON_OBJECT, v->type);
    json_value_free(v);
}

TEST_CASE(test_parse_object_simple)
{
    JsonValue* v = json_parse("{\"key\": \"value\"}");
    TEST_ASSERT_NOT_NULL(v);
    TEST_ASSERT_EQUAL_INT(JSON_OBJECT, v->type);
    JsonValue* val = json_object_get(v, "key");
    TEST_ASSERT_NOT_NULL(val);
    TEST_ASSERT_EQUAL_INT(JSON_STRING, val->type);
    TEST_ASSERT_EQUAL_STRING("value", json_as_string(val));
    json_value_free(v);
}

TEST_CASE(test_parse_object_multiple)
{
    JsonValue* v = json_parse("{\"name\":\"Alice\",\"age\":30,\"active\":true}");
    TEST_ASSERT_NOT_NULL(v);
    TEST_ASSERT_EQUAL_INT(JSON_OBJECT, v->type);
    TEST_ASSERT_EQUAL_STRING("Alice", json_as_string(json_object_get(v, "name")));
    TEST_ASSERT_EQUAL_DOUBLE(30.0, json_as_number(json_object_get(v, "age")), 0.001);
    TEST_ASSERT_EQUAL_INT(1, json_as_bool(json_object_get(v, "active")));
    json_value_free(v);
}

TEST_CASE(test_parse_object_nested)
{
    JsonValue* v = json_parse("{\"user\":{\"name\":\"Bob\",\"scores\":[95,87,92]}}");
    TEST_ASSERT_NOT_NULL(v);
    JsonValue* user = json_object_get(v, "user");
    TEST_ASSERT_NOT_NULL(user);
    TEST_ASSERT_EQUAL_STRING("Bob", json_as_string(json_object_get(user, "name")));
    JsonValue* scores = json_object_get(user, "scores");
    TEST_ASSERT_NOT_NULL(scores);
    TEST_ASSERT_EQUAL_INT(3, json_array_length(scores));
    TEST_ASSERT_EQUAL_DOUBLE(95.0, json_as_number(json_array_get(scores, 0)), 0.001);
    json_value_free(v);
}

TEST_CASE(test_parse_object_key_not_found)
{
    JsonValue* v = json_parse("{\"a\":1}");
    TEST_ASSERT_NOT_NULL(v);
    JsonValue* val = json_object_get(v, "nonexistent");
    TEST_ASSERT_NULL(val);
    json_value_free(v);
}

/* ============================================================
 * 测试: JSON 构建 API
 * ============================================================ */
TEST_CASE(test_build_response)
{
    char* resp = json_build_response("ok", "success", "{\"id\":123}");
    TEST_ASSERT_NOT_NULL(resp);
    /* 验证能解析回来 */
    JsonValue* v = json_parse(resp);
    TEST_ASSERT_NOT_NULL(v);
    TEST_ASSERT_EQUAL_INT(JSON_OBJECT, v->type);
    TEST_ASSERT_EQUAL_STRING("ok", json_as_string(json_object_get(v, "status")));
    TEST_ASSERT_EQUAL_STRING("success", json_as_string(json_object_get(v, "message")));
    json_value_free(v);
    free(resp);
}

TEST_CASE(test_build_error)
{
    char* err = json_build_error("出错了");
    TEST_ASSERT_NOT_NULL(err);
    JsonValue* v = json_parse(err);
    TEST_ASSERT_NOT_NULL(v);
    TEST_ASSERT_EQUAL_STRING("error", json_as_string(json_object_get(v, "status")));
    TEST_ASSERT_EQUAL_STRING("出错了", json_as_string(json_object_get(v, "message")));
    json_value_free(v);
    free(err);
}

TEST_CASE(test_build_success)
{
    char* ok = json_build_success("{\"result\":\"done\"}");
    TEST_ASSERT_NOT_NULL(ok);
    JsonValue* v = json_parse(ok);
    TEST_ASSERT_NOT_NULL(v);
    TEST_ASSERT_EQUAL_STRING("ok", json_as_string(json_object_get(v, "status")));
    TEST_ASSERT_EQUAL_STRING("done", json_as_string(
        json_object_get(json_object_get(v, "data"), "result")));
    json_value_free(v);
    free(ok);
}

/* ============================================================
 * 测试: 字符串转义
 * ============================================================ */
TEST_CASE(test_escape_string_normal)
{
    char* escaped = json_escape_string("hello");
    TEST_ASSERT_NOT_NULL(escaped);
    TEST_ASSERT_EQUAL_STRING("hello", escaped);
    free(escaped);
}

TEST_CASE(test_escape_string_with_quotes)
{
    char* escaped = json_escape_string("say \"hello\"");
    TEST_ASSERT_NOT_NULL(escaped);
    TEST_ASSERT(strstr(escaped, "\\\"") != NULL);
    free(escaped);
}

TEST_CASE(test_escape_string_with_backslash)
{
    char* escaped = json_escape_string("a\\b");
    TEST_ASSERT_NOT_NULL(escaped);
    TEST_ASSERT(strstr(escaped, "\\\\") != NULL);
    free(escaped);
}

TEST_CASE(test_escape_string_with_control)
{
    char* escaped = json_escape_string("line1\nline2\t");
    TEST_ASSERT_NOT_NULL(escaped);
    TEST_ASSERT(strstr(escaped, "\\n") != NULL);
    TEST_ASSERT(strstr(escaped, "\\t") != NULL);
    free(escaped);
}

TEST_CASE(test_escape_string_empty)
{
    char* escaped = json_escape_string("");
    TEST_ASSERT_NOT_NULL(escaped);
    TEST_ASSERT_EQUAL_STRING("", escaped);
    free(escaped);
}

/* ============================================================
 * 测试: 提取辅助函数
 * ============================================================ */
TEST_CASE(test_extract_string)
{
    char* val = json_extract_string("{\"name\":\"Charlie\"}", "name");
    TEST_ASSERT_NOT_NULL(val);
    TEST_ASSERT_EQUAL_STRING("Charlie", val);
    free(val);
}

TEST_CASE(test_extract_number)
{
    double val = json_extract_number("{\"score\":98.5}", "score");
    TEST_ASSERT_EQUAL_DOUBLE(98.5, val, 0.001);
}

TEST_CASE(test_extract_not_found)
{
    char* val = json_extract_string("{\"a\":1}", "b");
    TEST_ASSERT_NULL(val);
}

/* ============================================================
 * 测试: 无效输入 (边界/安全)
 * ============================================================ */
TEST_CASE(test_parse_empty)
{
    JsonValue* v = json_parse("");
    TEST_ASSERT_NULL(v);
}

TEST_CASE(test_parse_invalid_json)
{
    JsonValue* v = json_parse("{invalid}");
    TEST_ASSERT_NULL(v);
}

TEST_CASE(test_parse_truncated)
{
    JsonValue* v = json_parse("{\"key\": ");
    TEST_ASSERT_NULL(v);
}

TEST_CASE(test_parse_unclosed_string)
{
    JsonValue* v = json_parse("\"unclosed");
    TEST_ASSERT_NULL(v);
}

TEST_CASE(test_parse_unclosed_array)
{
    JsonValue* v = json_parse("[1,2,3");
    TEST_ASSERT_NULL(v);
}

TEST_CASE(test_parse_unclosed_object)
{
    JsonValue* v = json_parse("{\"a\":1");
    TEST_ASSERT_NULL(v);
}

TEST_CASE(test_parse_null_input)
{
    JsonValue* v = json_parse(NULL);
    TEST_ASSERT_NULL(v);
}

/* ============================================================
 * 测试: 深度嵌套限制 (安全)
 * ============================================================ */
TEST_CASE(test_deep_nesting_limit)
{
    /* 构建 65 层嵌套的数组 [[[...]]], 用 65 个 ']' 正确闭合 */
    size_t depth = 65;
    size_t json_len = depth * 2;
    char* deep_json = (char*)malloc(json_len + 1);
    TEST_ASSERT_NOT_NULL(deep_json);
    memset(deep_json, '[', depth);
    memset(deep_json + depth, ']', depth);
    deep_json[json_len] = '\0';

    JsonValue* v = json_parse(deep_json);
    /* 深度超过 64 应返回 NULL 防止栈溢出 */
    TEST_ASSERT_NULL(v);
    free(deep_json);
}

TEST_CASE(test_near_depth_limit)
{
    /* 构建 63 层嵌套 (应能成功解析), 用 63 个 ']' 正确闭合 */
    size_t depth = 63;
    size_t json_len = depth * 2;
    char* deep_json = (char*)malloc(json_len + 1);
    TEST_ASSERT_NOT_NULL(deep_json);
    memset(deep_json, '[', depth);
    memset(deep_json + depth, ']', depth);
    deep_json[json_len] = '\0';

    JsonValue* v = json_parse(deep_json);
    TEST_ASSERT_NOT_NULL(v);
    json_value_free(v);
    free(deep_json);
}

/* ============================================================
 * 测试: 大字符串限制 (安全)
 * ============================================================ */
TEST_CASE(test_large_string_rejected)
{
    /* 尝试解析 2MB 字符串 (超过 1MB 限制) */
    size_t huge_len = 2 * 1024 * 1024 + 10;
    char* huge_json = (char*)malloc(huge_len + 3);
    TEST_ASSERT_NOT_NULL(huge_json);
    huge_json[0] = '"';
    memset(huge_json + 1, 'A', huge_len);
    huge_json[huge_len + 1] = '"';
    huge_json[huge_len + 2] = '\0';

    JsonValue* v = json_parse(huge_json);
    TEST_ASSERT_NULL(v);
    free(huge_json);
}

/* ============================================================
 * 测试: 大数组元素限制 (安全)
 * ============================================================ */
TEST_CASE(test_large_array_rejected)
{
    /* 构建超过 256K 元素的数组 (JSON_MAX_ELEMENT_COUNT = 262144) */
    size_t elem_count = 270000;
    size_t buf_size = elem_count * 3 + 10;
    char* large_arr = (char*)malloc(buf_size);
    TEST_ASSERT_NOT_NULL(large_arr);

    char* p = large_arr;
    *p++ = '[';
    for (size_t i = 0; i < elem_count; i++) {
        if (i > 0) *p++ = ',';
        *p++ = '1';
    }
    *p++ = ']';
    *p = '\0';

    JsonValue* v = json_parse(large_arr);
    TEST_ASSERT_NULL(v);
    free(large_arr);
}

/* ============================================================
 * 测试: 大数据对象拒绝 (安全)
 * ============================================================ */
TEST_CASE(test_large_object_rejected)
{
    /* 构建超过 256K 个字段的对象 (JSON_MAX_ELEMENT_COUNT = 262144) */
    size_t field_count = 270000;
    /* 每个最大字段 "\"k999999\":1," 约 14 字节, 预留 16 */
    size_t buf_size = field_count * 16 + 16;
    char* large_obj = (char*)malloc(buf_size);
    TEST_ASSERT_NOT_NULL(large_obj);

    char* p = large_obj;
    *p++ = '{';
    for (size_t i = 0; i < field_count; i++) {
        if (i > 0) *p++ = ',';
        size_t remaining = buf_size - (size_t)(p - large_obj);
        int n = snprintf(p, remaining, "\"k%zu\":1", i);
        if (n > 0 && (size_t)n < remaining) p += n;
        else break;
    }
    *p++ = '}';
    *p = '\0';

    JsonValue* v = json_parse(large_obj);
    TEST_ASSERT_NULL(v);
    free(large_obj);
}

/* ============================================================
 * 测试注册
 * ============================================================ */
void run_json_parser_tests(void)
{
    setup();

    printf("\n=== JSON 解析器单元测试 ===\n");

    /* 基本类型 */
    RUN_TEST(test_parse_null);
    RUN_TEST(test_parse_null_whitespace);
    RUN_TEST(test_parse_true);
    RUN_TEST(test_parse_false);
    RUN_TEST(test_parse_integer);
    RUN_TEST(test_parse_negative);
    RUN_TEST(test_parse_float);
    RUN_TEST(test_parse_scientific);
    RUN_TEST(test_parse_zero);

    /* 字符串 */
    RUN_TEST(test_parse_string_empty);
    RUN_TEST(test_parse_string_hello);
    RUN_TEST(test_parse_string_unicode);
    RUN_TEST(test_parse_string_escaped);

    /* 数组 */
    RUN_TEST(test_parse_array_empty);
    RUN_TEST(test_parse_array_numbers);
    RUN_TEST(test_parse_array_mixed);

    /* 对象 */
    RUN_TEST(test_parse_object_empty);
    RUN_TEST(test_parse_object_simple);
    RUN_TEST(test_parse_object_multiple);
    RUN_TEST(test_parse_object_nested);
    RUN_TEST(test_parse_object_key_not_found);

    /* 构建 API */
    RUN_TEST(test_build_response);
    RUN_TEST(test_build_error);
    RUN_TEST(test_build_success);

    /* 字符串转义 */
    RUN_TEST(test_escape_string_normal);
    RUN_TEST(test_escape_string_with_quotes);
    RUN_TEST(test_escape_string_with_backslash);
    RUN_TEST(test_escape_string_with_control);
    RUN_TEST(test_escape_string_empty);

    /* 提取辅助 */
    RUN_TEST(test_extract_string);
    RUN_TEST(test_extract_number);
    RUN_TEST(test_extract_not_found);

    /* 无效输入 */
    RUN_TEST(test_parse_empty);
    RUN_TEST(test_parse_invalid_json);
    RUN_TEST(test_parse_truncated);
    RUN_TEST(test_parse_unclosed_string);
    RUN_TEST(test_parse_unclosed_array);
    RUN_TEST(test_parse_unclosed_object);
    RUN_TEST(test_parse_null_input);

    /* 安全边界 */
    RUN_TEST(test_deep_nesting_limit);
    RUN_TEST(test_near_depth_limit);
    RUN_TEST(test_large_string_rejected);
    RUN_TEST(test_large_array_rejected);
    RUN_TEST(test_large_object_rejected);
}
