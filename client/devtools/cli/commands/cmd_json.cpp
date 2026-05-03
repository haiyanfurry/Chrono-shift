/**
 * cmd_json.cpp — JSON 解析/格式化命令
 * 对应 debug_cli.c:1093 cmd_json_parse + 1148 cmd_json_pretty
 *
 * C++23 转换: std::println, extern "C"
 */
#include "../devtools_cli.hpp"

#include <print>     // std::println
#include <cstdio>    // std::snprintf (used by print_json indirectly)
#include <cstring>   // std::strlen

/* ============================================================
 * JsonValue / json_parse / json_value_free
 * 由 core 或外部库提供
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
 * json-parse 命令 - 解析并验证 JSON
 * ============================================================ */
static int cmd_json_parse(int argc, char** argv)
{
    if (argc < 1) {
        std::println(stderr, "用法: json-parse <json_string>");
        std::println(stderr, "  解析并验证 JSON 字符串的合法性");
        return -1;
    }

    const char* input = argv[0];
    std::println("[*] 输入JSON ({} 字节): {}", std::strlen(input), input);
    std::println("");

    JsonValue* val = json_parse(input);
    if (!val) {
        std::println("[-] JSON 解析失败: 语法无效");
        std::println("    可能的原因:");
        std::println("      - 缺少括号或引号");
        std::println("      - 多余的逗号");
        std::println("      - 字符串未正确转义");
        return -1;
    }

    std::println("[+] JSON 解析成功!");
    switch (val->type) {
        case JSON_OBJECT:
            std::println("    类型: OBJECT");
            std::println("    键值对数量: {}", val->object.count);
            break;
        case JSON_ARRAY:
            std::println("    类型: ARRAY");
            std::println("    元素数量: {}", val->array.count);
            break;
        case JSON_STRING:
            std::println("    类型: STRING");
            std::println("    值: {}", val->string_val);
            break;
        case JSON_NUMBER:
            std::println("    类型: NUMBER");
            std::println("    值: {}", val->number_val);
            break;
        case JSON_BOOL:
            std::println("    类型: BOOL");
            std::println("    值: {}", val->bool_val ? "true" : "false");
            break;
        case JSON_NULL:
            std::println("    类型: NULL");
            break;
        default:
            std::println("    类型: UNKNOWN");
            break;
    }

    json_value_free(val);
    return 0;
}

/* ============================================================
 * json-pretty 命令 - 格式化 JSON
 * ============================================================ */
static int cmd_json_pretty(int argc, char** argv)
{
    if (argc < 1) {
        std::println(stderr, "用法: json-pretty <json_string>");
        std::println(stderr, "  格式化输出 JSON 字符串");
        return -1;
    }

    const char* input = argv[0];
    std::println("[*] 格式化后的 JSON:");
    std::println("");

    /* 先验证 JSON */
    JsonValue* val = json_parse(input);
    if (!val) {
        /* 即使解析失败, 也尝试简单格式化 */
        std::println("[-] 警告: JSON 语法可能无效, 尝试直接格式化");
        print_json(input, 0);
        return -1;
    }

    print_json(input, 0);
    json_value_free(val);
    return 0;
}

extern "C" int init_cmd_json_parse(void)
{
    register_command("json-parse",
        "解析并验证 JSON 字符串",
        "json-parse <json_string>",
        cmd_json_parse);
    return 0;
}

extern "C" int init_cmd_json_pretty(void)
{
    register_command("json-pretty",
        "格式化输出 JSON 字符串",
        "json-pretty <json_string>",
        cmd_json_pretty);
    return 0;
}
