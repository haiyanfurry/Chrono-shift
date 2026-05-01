/**
 * test_http_parse.c — HTTP 解析器单元测试
 *
 * 测试 http_parse.c 中的公开辅助函数:
 * - http_get_header_value()     — 大小写不敏感的头部查找
 * - http_extract_bearer_token() — Bearer token 提取
 */
#include "unity.h"
#include "http_server.h"
#include <string.h>

/* ============================================================
 * 测试: http_get_header_value
 * ============================================================ */
TEST_CASE(test_header_value_found_exact)
{
    char headers[64][2][1024] = {{{"Content-Type"}, {"application/json"}}};
    const char* val = http_get_header_value(headers, 1, "Content-Type");
    TEST_ASSERT_NOT_NULL(val);
    TEST_ASSERT_EQUAL_STRING("application/json", val);
}

TEST_CASE(test_header_value_case_insensitive)
{
    char headers[64][2][1024] = {{{"CONTENT-TYPE"}, {"text/html"}}};
    const char* val = http_get_header_value(headers, 1, "content-type");
    TEST_ASSERT_NOT_NULL(val);
    TEST_ASSERT_EQUAL_STRING("text/html", val);
}

TEST_CASE(test_header_value_mixed_case)
{
    char headers[64][2][1024] = {{{"content-type"}, {"text/plain"}}};
    const char* val = http_get_header_value(headers, 1, "Content-Type");
    TEST_ASSERT_NOT_NULL(val);
    TEST_ASSERT_EQUAL_STRING("text/plain", val);
}

TEST_CASE(test_header_value_not_found)
{
    char headers[64][2][1024] = {{{"Host"}, {"example.com"}}};
    const char* val = http_get_header_value(headers, 1, "Accept");
    TEST_ASSERT_NULL(val);
}

TEST_CASE(test_header_value_empty_headers)
{
    const char* val = http_get_header_value(NULL, 0, "Host");
    TEST_ASSERT_NULL(val);
}

TEST_CASE(test_header_value_null_key)
{
    char headers[64][2][1024] = {{{"Host"}, {"example.com"}}};
    const char* val = http_get_header_value(headers, 1, NULL);
    TEST_ASSERT_NULL(val);
}

TEST_CASE(test_header_value_multiple_headers)
{
    char headers[64][2][1024] = {
        {{"Host"}, {"example.com"}},
        {{"Content-Type"}, {"application/json"}},
        {{"Authorization"}, {"Bearer mytoken"}}
    };
    TEST_ASSERT_EQUAL_STRING("example.com",
        http_get_header_value(headers, 3, "Host"));
    TEST_ASSERT_EQUAL_STRING("application/json",
        http_get_header_value(headers, 3, "Content-Type"));
    TEST_ASSERT_EQUAL_STRING("Bearer mytoken",
        http_get_header_value(headers, 3, "Authorization"));
}

/* ============================================================
 * 测试: http_extract_bearer_token
 * ============================================================ */
TEST_CASE(test_bearer_token_valid)
{
    char headers[64][2][1024] = {{{"Authorization"}, {"Bearer abc123token"}}};
    const char* token = http_extract_bearer_token(headers, 1);
    TEST_ASSERT_NOT_NULL(token);
    TEST_ASSERT_EQUAL_STRING("abc123token", token);
}

TEST_CASE(test_bearer_token_no_bearer)
{
    char headers[64][2][1024] = {{{"Authorization"}, {"Basic dXNlcjpwYXNz"}}};
    const char* token = http_extract_bearer_token(headers, 1);
    TEST_ASSERT_NULL(token);
}

TEST_CASE(test_bearer_token_missing_header)
{
    char headers[64][2][1024] = {{{"Host"}, {"example.com"}}};
    const char* token = http_extract_bearer_token(headers, 1);
    TEST_ASSERT_NULL(token);
}

TEST_CASE(test_bearer_token_case_insensitive)
{
    char headers[64][2][1024] = {{{"authorization"}, {"bearER token123"}}};
    const char* token = http_extract_bearer_token(headers, 1);
    TEST_ASSERT_NOT_NULL(token);
    TEST_ASSERT_EQUAL_STRING("token123", token);
}

TEST_CASE(test_bearer_token_empty_after_prefix)
{
    char headers[64][2][1024] = {{{"Authorization"}, {"Bearer "}}};
    const char* token = http_extract_bearer_token(headers, 1);
    TEST_ASSERT_NOT_NULL(token);
    TEST_ASSERT_EQUAL_STRING("", token);
}

/* ============================================================
 * 测试注册
 * ============================================================ */
void run_http_parse_tests(void)
{
    printf("\n=== HTTP 解析器单元测试 ===\n");

    RUN_TEST(test_header_value_found_exact);
    RUN_TEST(test_header_value_case_insensitive);
    RUN_TEST(test_header_value_mixed_case);
    RUN_TEST(test_header_value_not_found);
    RUN_TEST(test_header_value_empty_headers);
    RUN_TEST(test_header_value_null_key);
    RUN_TEST(test_header_value_multiple_headers);

    RUN_TEST(test_bearer_token_valid);
    RUN_TEST(test_bearer_token_no_bearer);
    RUN_TEST(test_bearer_token_missing_header);
    RUN_TEST(test_bearer_token_case_insensitive);
    RUN_TEST(test_bearer_token_empty_after_prefix);
}
