/**
 * test_runner.c — 单元测试主运行器
 *
 * 编译:
 *   gcc -std=c99 -I../include -I. test_runner.c ../src/json_parser.c
 *       ../src/http_parse.c -o run_tests -lm
 *
 * 运行:
 *   ./run_tests
 */
#include "unity.h"
#include <stdio.h>
#include <setjmp.h>

/* ============================================================
 * 全局状态 (供 unity.h 使用)
 * ============================================================ */
int g_unity_test_count = 0;
int g_unity_pass_count = 0;
int g_unity_fail_count = 0;
jmp_buf g_unity_abort_jmp;

/* 外部测试注册函数 */
extern void run_json_parser_tests(void);
extern void run_http_parse_tests(void);

int main(void)
{
    printf("========================================\n");
    printf("  Chrono-shift 单元测试套件\n");
    printf("========================================\n");

    run_json_parser_tests();
    run_http_parse_tests();

    printf("\n========================================\n");
    printf("  总计: %d | 通过: %d | 失败: %d\n",
           g_unity_test_count, g_unity_pass_count, g_unity_fail_count);
    printf("========================================\n");

    return g_unity_fail_count > 0 ? 1 : 0;
}
