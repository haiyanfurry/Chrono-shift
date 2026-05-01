/**
 * unity.h — 微型单元测试框架 (Unity 兼容接口子集)
 * 原始 Unity 框架: https://github.com/ThrowTheSwitch/Unity
 * 许可证: MIT
 *
 * 本文件是 Unity 框架的最小化实现，仅包含本项目所需的断言宏。
 */
#ifndef CHRONO_UNITY_H
#define CHRONO_UNITY_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <setjmp.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 全局状态
 * ============================================================ */
extern int g_unity_test_count;
extern int g_unity_pass_count;
extern int g_unity_fail_count;
extern jmp_buf g_unity_abort_jmp;

/* ============================================================
 * 测试注册与运行
 * ============================================================ */

#define TEST_CASE(name)                             \
    void name(void);                                \
    void name(void)

#define RUN_TEST(name) do {                         \
        g_unity_test_count++;                       \
        printf("  TEST: %s ... ", #name);           \
        fflush(stdout);                             \
        if (setjmp(g_unity_abort_jmp) == 0) {       \
            name();                                 \
            g_unity_pass_count++;                   \
            printf("PASS\n");                       \
        }                                           \
    } while (0)

/* ============================================================
 * 断言宏
 * ============================================================ */

#define TEST_FAIL(msg) do {                         \
        g_unity_fail_count++;                       \
        printf("FAIL: %s (%s:%d)\n", msg,           \
               __FILE__, __LINE__);                 \
        longjmp(g_unity_abort_jmp, 1);              \
    } while (0)

#define TEST_ASSERT(condition) do {                 \
        if (!(condition)) {                         \
            g_unity_fail_count++;                   \
            printf("FAIL: '%s' is FALSE (%s:%d)\n", \
                   #condition, __FILE__, __LINE__); \
            longjmp(g_unity_abort_jmp, 1);          \
        }                                           \
    } while (0)

#define TEST_ASSERT_TRUE(condition)  TEST_ASSERT(condition)
#define TEST_ASSERT_FALSE(condition) TEST_ASSERT(!(condition))
#define TEST_ASSERT_NOT_NULL(ptr)    TEST_ASSERT((ptr) != NULL)
#define TEST_ASSERT_NULL(ptr)        TEST_ASSERT((ptr) == NULL)

#define TEST_ASSERT_EQUAL_INT(expected, actual) do {            \
        int _e = (expected); int _a = (actual);                 \
        if (_e != _a) {                                         \
            g_unity_fail_count++;                               \
            printf("FAIL: expected %d, got %d (%s:%d)\n",       \
                   _e, _a, __FILE__, __LINE__);                 \
            longjmp(g_unity_abort_jmp, 1);                      \
        }                                                       \
    } while (0)

#define TEST_ASSERT_EQUAL_UINT(expected, actual) do {           \
        unsigned int _e = (unsigned int)(expected);             \
        unsigned int _a = (unsigned int)(actual);               \
        if (_e != _a) {                                         \
            g_unity_fail_count++;                               \
            printf("FAIL: expected %u, got %u (%s:%d)\n",       \
                   _e, _a, __FILE__, __LINE__);                 \
            longjmp(g_unity_abort_jmp, 1);                      \
        }                                                       \
    } while (0)

#define TEST_ASSERT_EQUAL_SIZE(expected, actual) do {           \
        size_t _e = (size_t)(expected);                         \
        size_t _a = (size_t)(actual);                           \
        if (_e != _a) {                                         \
            g_unity_fail_count++;                               \
            printf("FAIL: expected %zu, got %zu (%s:%d)\n",     \
                   _e, _a, __FILE__, __LINE__);                 \
            longjmp(g_unity_abort_jmp, 1);                      \
        }                                                       \
    } while (0)

#define TEST_ASSERT_EQUAL_STRING(expected, actual) do {         \
        const char* _e = (expected);                            \
        const char* _a = (actual);                              \
        if ((_e == NULL && _a != NULL) ||                       \
            (_e != NULL && _a == NULL) ||                       \
            (_e != NULL && _a != NULL && strcmp(_e, _a) != 0)) {\
            g_unity_fail_count++;                               \
            printf("FAIL: expected \"%s\", got \"%s\" (%s:%d)\n",\
                   _e ? _e : "(null)",                          \
                   _a ? _a : "(null)",                          \
                   __FILE__, __LINE__);                         \
            longjmp(g_unity_abort_jmp, 1);                      \
        }                                                       \
    } while (0)

#define TEST_ASSERT_EQUAL_DOUBLE(expected, actual, epsilon) do {\
        double _e = (double)(expected);                         \
        double _a = (double)(actual);                           \
        if (fabs(_e - _a) > (double)(epsilon)) {                \
            g_unity_fail_count++;                               \
            printf("FAIL: expected %f, got %f (eps=%f) (%s:%d)\n",\
                   _e, _a, (double)(epsilon),                   \
                   __FILE__, __LINE__);                         \
            longjmp(g_unity_abort_jmp, 1);                      \
        }                                                       \
    } while (0)

#define TEST_ASSERT_EQUAL_PTR(expected, actual) do {            \
        const void* _e = (const void*)(expected);               \
        const void* _a = (const void*)(actual);                 \
        if (_e != _a) {                                         \
            g_unity_fail_count++;                               \
            printf("FAIL: expected %p, got %p (%s:%d)\n",       \
                   _e, _a, __FILE__, __LINE__);                 \
            longjmp(g_unity_abort_jmp, 1);                      \
        }                                                       \
    } while (0)

#ifdef __cplusplus
}
#endif

#endif /* CHRONO_UNITY_H */
