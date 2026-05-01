/**
 * test_security.cpp — 安全模块单元测试 (C++)
 *
 * 测试 RateLimiter, CsrfProtector, InputSanitizer 三个安全模块。
 * 使用 unity.h 测试框架 (已 extern "C" 包装，兼容 C++)。
 *
 * 编译:
 *   g++ -std=c++17 -I../src -I../include -I. test_security.cpp
 *       ../src/security/SecurityManager.cpp
 *       ../src/util/StringUtils.cpp
 *       ../src/util/Logger.cpp
 *       -o run_security_cpp_tests
 *
 * 运行:
 *   ./run_security_cpp_tests
 */
#include "unity.h"
#include "security/SecurityManager.h"
#include <string>
#include <thread>
#include <chrono>
#include <vector>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 全局状态 (供 unity.h 使用)
 * ============================================================ */
int g_unity_test_count = 0;
int g_unity_pass_count = 0;
int g_unity_fail_count = 0;
jmp_buf g_unity_abort_jmp;

#ifdef __cplusplus
}
#endif

// ============================================================
// 测试: RateLimiter
// ============================================================

TEST_CASE(test_rate_limiter_allow_first_request)
{
    chrono::security::RateLimiter limiter(5, 1000);
    TEST_ASSERT_TRUE(limiter.allow("user1"));
}

TEST_CASE(test_rate_limiter_allow_within_limit)
{
    chrono::security::RateLimiter limiter(3, 1000);
    TEST_ASSERT_TRUE(limiter.allow("user1"));  // 1/3
    TEST_ASSERT_TRUE(limiter.allow("user1"));  // 2/3
    TEST_ASSERT_TRUE(limiter.allow("user1"));  // 3/3
    TEST_ASSERT_FALSE(limiter.allow("user1")); // 4/3 — 被限
}

TEST_CASE(test_rate_limiter_different_keys_independent)
{
    chrono::security::RateLimiter limiter(2, 1000);
    TEST_ASSERT_TRUE(limiter.allow("user_a")); // 1/2
    TEST_ASSERT_TRUE(limiter.allow("user_b")); // 1/2
    TEST_ASSERT_TRUE(limiter.allow("user_a")); // 2/2 — user_a 满
    TEST_ASSERT_FALSE(limiter.allow("user_a")); // user_a 被限
    TEST_ASSERT_TRUE(limiter.allow("user_b"));  // user_b 仍有额度
}

TEST_CASE(test_rate_limiter_window_expiry)
{
    chrono::security::RateLimiter limiter(1, 100); // 1 次 / 100ms
    TEST_ASSERT_TRUE(limiter.allow("key"));   // 1/1
    TEST_ASSERT_FALSE(limiter.allow("key"));  // 被限
    std::this_thread::sleep_for(std::chrono::milliseconds(150)); // 等窗口过期
    TEST_ASSERT_TRUE(limiter.allow("key"));   // 窗口重置，通过
}

TEST_CASE(test_rate_limiter_cleanup)
{
    chrono::security::RateLimiter limiter(5, 50); // 短窗口
    limiter.allow("a");
    limiter.allow("b");
    limiter.allow("c");
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 等窗口过期
    limiter.cleanup();  // 应清理所有过期条目
    // 清理后，所有 key 应该都能通过（视为新条目）
    TEST_ASSERT_TRUE(limiter.allow("a"));
    TEST_ASSERT_TRUE(limiter.allow("b"));
    TEST_ASSERT_TRUE(limiter.allow("c"));
}

TEST_CASE(test_rate_limiter_high_volume)
{
    chrono::security::RateLimiter limiter(1000, 60000);
    for (int i = 0; i < 1000; i++) {
        TEST_ASSERT_TRUE(limiter.allow("high_vol_user"));
    }
    TEST_ASSERT_FALSE(limiter.allow("high_vol_user")); // 1001 次 — 被限
}

TEST_CASE(test_rate_limiter_empty_key)
{
    chrono::security::RateLimiter limiter(5, 1000);
    TEST_ASSERT_TRUE(limiter.allow(""));
    TEST_ASSERT_TRUE(limiter.allow(""));
}

// ============================================================
// 测试: CsrfProtector
// ============================================================

TEST_CASE(test_csrf_generate_token_not_empty)
{
    std::string token = chrono::security::CsrfProtector::generate_token();
    TEST_ASSERT_FALSE(token.empty());
}

TEST_CASE(test_csrf_generate_token_unique)
{
    std::string t1 = chrono::security::CsrfProtector::generate_token();
    std::string t2 = chrono::security::CsrfProtector::generate_token();
    TEST_ASSERT_TRUE(t1 != t2);
}

TEST_CASE(test_csrf_validate_correct_token)
{
    std::string token = chrono::security::CsrfProtector::generate_token();
    TEST_ASSERT_TRUE(chrono::security::CsrfProtector::validate_token(token, token));
}

TEST_CASE(test_csrf_validate_wrong_token)
{
    std::string token = chrono::security::CsrfProtector::generate_token();
    TEST_ASSERT_FALSE(chrono::security::CsrfProtector::validate_token(token, "wrong-token"));
}

TEST_CASE(test_csrf_validate_empty_token)
{
    TEST_ASSERT_FALSE(chrono::security::CsrfProtector::validate_token("", ""));
    TEST_ASSERT_FALSE(chrono::security::CsrfProtector::validate_token("", "some-token"));
}

TEST_CASE(test_csrf_generate_token_format)
{
    std::string token = chrono::security::CsrfProtector::generate_token();
    // UUID 格式: xxxxxxxx-xxxx-4xxx-xxxx-xxxxxxxxxxxx (36 字符)
    TEST_ASSERT_EQUAL_INT(36, (int)token.size());
    TEST_ASSERT_EQUAL_INT('-', token[8]);
    TEST_ASSERT_EQUAL_INT('-', token[13]);
    TEST_ASSERT_EQUAL_INT('-', token[18]);
    TEST_ASSERT_EQUAL_INT('-', token[23]);
}

// ============================================================
// 测试: InputSanitizer
// ============================================================

TEST_CASE(test_sanitize_username_normal)
{
    std::string result = chrono::security::InputSanitizer::sanitize_username("john_doe-123");
    TEST_ASSERT_EQUAL_STRING("john_doe-123", result.c_str());
}

TEST_CASE(test_sanitize_username_remove_special_chars)
{
    std::string result = chrono::security::InputSanitizer::sanitize_username("john@doe!<>");
    TEST_ASSERT_EQUAL_STRING("johndoe", result.c_str());
}

TEST_CASE(test_sanitize_username_truncate_long)
{
    std::string long_name = "a very very very very very very very long username here!";
    std::string result = chrono::security::InputSanitizer::sanitize_username(long_name);
    TEST_ASSERT_TRUE(result.size() <= 32);
}

TEST_CASE(test_sanitize_username_empty)
{
    std::string result = chrono::security::InputSanitizer::sanitize_username("");
    TEST_ASSERT_EQUAL_STRING("", result.c_str());
}

TEST_CASE(test_sanitize_display_name_normal)
{
    std::string result = chrono::security::InputSanitizer::sanitize_display_name("Hello World 你好");
    TEST_ASSERT_TRUE(result.find("Hello") != std::string::npos);
}

TEST_CASE(test_sanitize_display_name_remove_html)
{
    std::string result = chrono::security::InputSanitizer::sanitize_display_name("<script>alert('xss')</script>");
    TEST_ASSERT_EQUAL_STRING("scriptalert(xss)/script", result.c_str());
}

TEST_CASE(test_sanitize_message_xss_prevention)
{
    std::string result = chrono::security::InputSanitizer::sanitize_message("<script>alert(1)</script>");
    TEST_ASSERT_TRUE(result.find("<") == std::string::npos);
    TEST_ASSERT_TRUE(result.find("&lt;") != std::string::npos);
}

TEST_CASE(test_sanitize_message_html_escape)
{
    std::string result = chrono::security::InputSanitizer::sanitize_message("a & b < c > d \"quote'");
    TEST_ASSERT_TRUE(result.find("&amp;") != std::string::npos);
    TEST_ASSERT_TRUE(result.find("&lt;") != std::string::npos);
    TEST_ASSERT_TRUE(result.find("&gt;") != std::string::npos);
    TEST_ASSERT_TRUE(result.find("&quot;") != std::string::npos);
}

TEST_CASE(test_check_password_strength_too_short)
{
    std::string result = chrono::security::InputSanitizer::check_password_strength("Ab1!");
    TEST_ASSERT_FALSE(result.empty()); // 太短
}

TEST_CASE(test_check_password_strength_missing_upper)
{
    std::string result = chrono::security::InputSanitizer::check_password_strength("abcdef1!@");
    TEST_ASSERT_FALSE(result.empty()); // 缺少大写
}

TEST_CASE(test_check_password_strength_missing_lower)
{
    std::string result = chrono::security::InputSanitizer::check_password_strength("ABCDEF1!@");
    TEST_ASSERT_FALSE(result.empty()); // 缺少小写
}

TEST_CASE(test_check_password_strength_missing_digit)
{
    std::string result = chrono::security::InputSanitizer::check_password_strength("Abcdefgh!@");
    TEST_ASSERT_FALSE(result.empty()); // 缺少数字
}

TEST_CASE(test_check_password_strength_missing_special)
{
    std::string result = chrono::security::InputSanitizer::check_password_strength("Abcdefgh1");
    TEST_ASSERT_FALSE(result.empty()); // 缺少特殊字符
}

TEST_CASE(test_check_password_strength_valid)
{
    std::string result = chrono::security::InputSanitizer::check_password_strength("Abcd1234!@#");
    TEST_ASSERT_TRUE(result.empty()); // 通过
}

TEST_CASE(test_check_password_strength_too_long)
{
    std::string long_pwd(200, 'A');
    long_pwd += "1!a";
    std::string result = chrono::security::InputSanitizer::check_password_strength(long_pwd);
    TEST_ASSERT_FALSE(result.empty()); // 超长
}

TEST_CASE(test_is_valid_email_normal)
{
    TEST_ASSERT_TRUE(chrono::security::InputSanitizer::is_valid_email("user@example.com"));
    TEST_ASSERT_TRUE(chrono::security::InputSanitizer::is_valid_email("user.name+tag@example.co.uk"));
}

TEST_CASE(test_is_valid_email_invalid)
{
    TEST_ASSERT_FALSE(chrono::security::InputSanitizer::is_valid_email(""));
    TEST_ASSERT_FALSE(chrono::security::InputSanitizer::is_valid_email("not-an-email"));
    TEST_ASSERT_FALSE(chrono::security::InputSanitizer::is_valid_email("@example.com"));
    TEST_ASSERT_FALSE(chrono::security::InputSanitizer::is_valid_email("user@"));
}

TEST_CASE(test_escape_html_special_chars)
{
    std::string result = chrono::security::InputSanitizer::escape_html("<tag> & \"quote'");
    TEST_ASSERT_EQUAL_STRING("&lt;tag&gt; &amp; &quot;quote&apos;", result.c_str());
}

TEST_CASE(test_escape_html_no_special)
{
    std::string result = chrono::security::InputSanitizer::escape_html("Hello, 世界!");
    TEST_ASSERT_EQUAL_STRING("Hello, 世界!", result.c_str());
}

TEST_CASE(test_escape_html_empty)
{
    std::string result = chrono::security::InputSanitizer::escape_html("");
    TEST_ASSERT_EQUAL_STRING("", result.c_str());
}

// ============================================================
// 测试注册
// ============================================================
extern "C" void run_security_cpp_tests(void)
{
    printf("\n=== 安全模块单元测试 (C++) ===\n");

    // RateLimiter
    printf("\n--- RateLimiter ---\n");
    RUN_TEST(test_rate_limiter_allow_first_request);
    RUN_TEST(test_rate_limiter_allow_within_limit);
    RUN_TEST(test_rate_limiter_different_keys_independent);
    RUN_TEST(test_rate_limiter_window_expiry);
    RUN_TEST(test_rate_limiter_cleanup);
    RUN_TEST(test_rate_limiter_high_volume);
    RUN_TEST(test_rate_limiter_empty_key);

    // CsrfProtector
    printf("\n--- CsrfProtector ---\n");
    RUN_TEST(test_csrf_generate_token_not_empty);
    RUN_TEST(test_csrf_generate_token_unique);
    RUN_TEST(test_csrf_validate_correct_token);
    RUN_TEST(test_csrf_validate_wrong_token);
    RUN_TEST(test_csrf_validate_empty_token);
    RUN_TEST(test_csrf_generate_token_format);

    // InputSanitizer
    printf("\n--- InputSanitizer ---\n");
    RUN_TEST(test_sanitize_username_normal);
    RUN_TEST(test_sanitize_username_remove_special_chars);
    RUN_TEST(test_sanitize_username_truncate_long);
    RUN_TEST(test_sanitize_username_empty);
    RUN_TEST(test_sanitize_display_name_normal);
    RUN_TEST(test_sanitize_display_name_remove_html);
    RUN_TEST(test_sanitize_message_xss_prevention);
    RUN_TEST(test_sanitize_message_html_escape);
    RUN_TEST(test_check_password_strength_too_short);
    RUN_TEST(test_check_password_strength_missing_upper);
    RUN_TEST(test_check_password_strength_missing_lower);
    RUN_TEST(test_check_password_strength_missing_digit);
    RUN_TEST(test_check_password_strength_missing_special);
    RUN_TEST(test_check_password_strength_valid);
    RUN_TEST(test_check_password_strength_too_long);
    RUN_TEST(test_is_valid_email_normal);
    RUN_TEST(test_is_valid_email_invalid);
    RUN_TEST(test_escape_html_special_chars);
    RUN_TEST(test_escape_html_no_special);
    RUN_TEST(test_escape_html_empty);
}

// ============================================================
// 主函数
// ============================================================
int main(void)
{
    printf("========================================\n");
    printf("  Chrono-shift 安全模块单元测试 (C++)\n");
    printf("========================================\n");

    run_security_cpp_tests();

    printf("\n========================================\n");
    printf("  总计: %d | 通过: %d | 失败: %d\n",
           g_unity_test_count, g_unity_pass_count, g_unity_fail_count);
    printf("========================================\n");

    return g_unity_fail_count > 0 ? 1 : 0;
}
