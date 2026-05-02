/**
 * cmd_json.c — JSON 解析/格式化命令
 * 对应 debug_cli.c:1093 cmd_json_parse + 1148 cmd_json_pretty
 */
#include "../devtools_cli.h"

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

extern JsonValue* json_parse(const char* input);
extern void json_value_free(JsonValue* val);

/* ============================================================
 * json-parse 命令 - 解析并验证 JSON
 * ============================================================ */
static int cmd_json_parse(int argc, char** argv)
{
    if (argc < 1) {
        fprintf(stderr, "用法: json-parse <json_string>\n");
        fprintf(stderr, "  解析并验证 JSON 字符串的合法性\n");
        return -1;
    }

    const char* input = argv[0];
    printf("[*] 输入JSON (%zu 字节): %s\n\n", strlen(input), input);

    JsonValue* val = json_parse(input);
    if (!val) {
        printf("[-] JSON 解析失败: 语法无效\n");
        printf("    可能的原因:\n");
        printf("      - 缺少括号或引号\n");
        printf("      - 多余的逗号\n");
        printf("      - 字符串未正确转义\n");
        return -1;
    }

    printf("[+] JSON 解析成功!\n");
    switch (val->type) {
        case JSON_OBJECT:
            printf("    类型: OBJECT\n");
            printf("    键值对数量: %zu\n", val->object.count);
            break;
        case JSON_ARRAY:
            printf("    类型: ARRAY\n");
            printf("    元素数量: %zu\n", val->array.count);
            break;
        case JSON_STRING:
            printf("    类型: STRING\n");
            printf("    值: %s\n", val->string_val);
            break;
        case JSON_NUMBER:
            printf("    类型: NUMBER\n");
            printf("    值: %g\n", val->number_val);
            break;
        case JSON_BOOL:
            printf("    类型: BOOL\n");
            printf("    值: %s\n", val->bool_val ? "true" : "false");
            break;
        case JSON_NULL:
            printf("    类型: NULL\n");
            break;
        default:
            printf("    类型: UNKNOWN\n");
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
        fprintf(stderr, "用法: json-pretty <json_string>\n");
        fprintf(stderr, "  格式化输出 JSON 字符串\n");
        return -1;
    }

    const char* input = argv[0];
    printf("[*] 格式化后的 JSON:\n\n");

    /* 先验证 JSON */
    JsonValue* val = json_parse(input);
    if (!val) {
        /* 即使解析失败, 也尝试简单格式化 */
        printf("[-] 警告: JSON 语法可能无效, 尝试直接格式化\n");
        print_json(input, 0);
        return -1;
    }

    print_json(input, 0);
    json_value_free(val);
    return 0;
}

int init_cmd_json_parse(void)
{
    register_command("json-parse",
        "解析并验证 JSON 字符串",
        "json-parse <json_string>",
        cmd_json_parse);
    return 0;
}

int init_cmd_json_pretty(void)
{
    register_command("json-pretty",
        "格式化输出 JSON 字符串",
        "json-pretty <json_string>",
        cmd_json_pretty);
    return 0;
}
