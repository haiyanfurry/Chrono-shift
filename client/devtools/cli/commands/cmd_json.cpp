/**
 * cmd_json.cpp �?JSON 解析/格式化命�?
 * 对应 debug_cli.c:1093 cmd_json_parse + 1148 cmd_json_pretty
 *
 * C++23 转换: std::println, extern "C"
 */
#include "../devtools_cli.hpp"

#include "print_compat.h"     // std::println
#include <cstdio>    // std::snprintf (used by print_json indirectly)
#include <cstring>   // std::strlen

namespace cli = chrono::client::cli;

/* ============================================================
 * JsonValue / json_parse / json_value_free
 * �?core 或外部库提供
 * ============================================================ */
typedef enum {
    JSON_NULL, JSON_BOOL, JSON_NUMBER, JSON_STRING, JSON_ARRAY, JSON_OBJECT
} JsonType;

typedef struct JsonValue {
    JsonType type;
    union {
        int    bool_val;
        double number_val;
        char*  string_val;
        struct { struct JsonValue* values; size_t count; } array;
        struct { char** keys; struct JsonValue* values; size_t count; } object;
    };
} JsonValue;

extern "C" {
extern JsonValue* json_parse(const char* input);
extern void json_value_free(JsonValue* val);
}

/* ============================================================
 * json-parse 命令 - 解析并验�?JSON
 * ============================================================ */
static int cmd_json_parse(int argc, char** argv)
{
    if (argc < 1) {
        cli::println(stderr, "用法: json-parse <json_string>");
        cli::println(stderr, "  解析并验�?JSON 字符串的合法�?);
        return -1;
    }

    const char* input = argv[0];
    cli::println("[*] 输入JSON ({} 字节): {}", std::strlen(input), input);
    cli::println("");

    JsonValue* val = json_parse(input);
    if (!val) {
        cli::println("[-] JSON 解析失败: 语法无效");
        cli::println("    可能的原�?");
        cli::println("      - 缺少括号或引�?);
        cli::println("      - 多余的逗号");
        cli::println("      - 字符串未正确转义");
        return -1;
    }

    cli::println("[+] JSON 解析成功!");
    switch (val->type) {
        case JSON_OBJECT:
            cli::println("    类型: OBJECT");
            cli::println("    键值对数量: {}", val->object.count);
            break;
        case JSON_ARRAY:
            cli::println("    类型: ARRAY");
            cli::println("    元素数量: {}", val->array.count);
            break;
        case JSON_STRING:
            cli::println("    类型: STRING");
            cli::println("    �? {}", val->string_val);
            break;
        case JSON_NUMBER:
            cli::println("    类型: NUMBER");
            cli::println("    �? {}", val->number_val);
            break;
        case JSON_BOOL:
            cli::println("    类型: BOOL");
            cli::println("    �? {}", val->bool_val ? "true" : "false");
            break;
        case JSON_NULL:
            cli::println("    类型: NULL");
            break;
        default:
            cli::println("    类型: UNKNOWN");
            break;
    }

    json_value_free(val);
    return 0;
}

/* ============================================================
 * json-pretty 命令 - 格式�?JSON
 * ============================================================ */
static int cmd_json_pretty(int argc, char** argv)
{
    if (argc < 1) {
        cli::println(stderr, "用法: json-pretty <json_string>");
        cli::println(stderr, "  格式化输�?JSON 字符�?);
        return -1;
    }

    const char* input = argv[0];
    cli::println("[*] 格式化后�?JSON:");
    cli::println("");

    /* 先验�?JSON */
    JsonValue* val = json_parse(input);
    if (!val) {
        /* 即使解析失败, 也尝试简单格式化 */
        cli::println("[-] 警告: JSON 语法可能无效, 尝试直接格式�?);
        cli::print_json(input, 0);
        return -1;
    }

    cli::print_json(input, 0);
    json_value_free(val);
    return 0;
}

extern "C" int init_cmd_json_parse(void)
{
    register_command("json-parse",
        "解析并验�?JSON 字符�?,
        "json-parse <json_string>",
        cmd_json_parse);
    return 0;
}

extern "C" int init_cmd_json_pretty(void)
{
    register_command("json-pretty",
        "格式化输�?JSON 字符�?,
        "json-pretty <json_string>",
        cmd_json_pretty);
    return 0;
}
