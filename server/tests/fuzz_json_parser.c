/**
 * fuzz_json_parser.c — JSON 解析器模糊测试入口
 *
 * 编译 (libFuzzer):
 *   clang -fsanitize=fuzzer,address -std=c99 -I../include
 *         fuzz_json_parser.c ../src/json_parser.c -o fuzz_json_parser
 *
 * 运行:
 *   ./fuzz_json_parser corpus/
 *
 * 注意: 需要 clang 编译器支持 libFuzzer (-fsanitize=fuzzer)
 */
#include "json_parser.h"
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/**
 * LLVM libFuzzer 入口函数
 *
 * 将模糊测试数据作为 JSON 字符串传入解析器，验证:
 * 1. 解析器不会崩溃 (由 ASAN 保障)
 * 2. 解析成功的结果可以安全释放
 * 3. 构建 API 对任意输入不会崩溃
 */
int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* 忽略空数据 */
    if (size == 0) return 0;

    /* 限制输入大小防止 OOM (最大 100KB) */
    if (size > 102400) return 0;

    /* 将模糊数据作为 JSON 字符串解析 */
    char* input = (char*)malloc(size + 1);
    if (!input) return 0;
    memcpy(input, data, size);
    input[size] = '\0';

    /* 解析 JSON */
    JsonValue* val = json_parse(input);

    if (val) {
        /* 验证可以安全遍历 (基本查询操作) */
        if (val->type == JSON_OBJECT) {
            /* 尝试获取不存在的 key (不应崩溃) */
            json_object_get(val, "__fuzz_test_key__");
        }
        if (val->type == JSON_ARRAY) {
            /* 访问越界索引 (不应崩溃) */
            json_array_get(val, 999999);
        }
        /* 释放树 */
        json_value_free(val);
    }

    /* 测试构建 API (不应崩溃) */
    {
        char* resp = json_build_response("ok", "fuzz", input);
        free(resp);
    }
    {
        char* err = json_build_error(input);
        free(err);
    }
    {
        char* ok = json_build_success(input);
        free(ok);
    }
    {
        char* escaped = json_escape_string(input);
        free(escaped);
    }

    free(input);
    return 0;  /* 非零值表示应丢弃此输入 */
}
