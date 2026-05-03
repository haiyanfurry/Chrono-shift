/**
 * json_parser.c — 轻量级 JSON 构建/解析器
 */
#include "json_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

/* ============================================================
 * 构建 API
 * ============================================================ */

static char* json_alloc_concat(const char* a, const char* b, const char* c)
{
    size_t la = a ? strlen(a) : 0;
    size_t lb = b ? strlen(b) : 0;
    size_t lc = c ? strlen(c) : 0;
    char* out = (char*)malloc(la + lb + lc + 1);
    if (!out) return NULL;
    char* p = out;
    if (la) { memcpy(p, a, la); p += la; }
    if (lb) { memcpy(p, b, lb); p += lb; }
    if (lc) { memcpy(p, c, lc); p += lc; }
    *p = '\0';
    return out;
}

char* json_build_response(const char* status, const char* message, const char* data_json)
{
    char buf[4096];
    int need_comma = (data_json && data_json[0]);
    int len = snprintf(buf, sizeof(buf),
        "{\"status\":\"%s\",\"message\":\"%s\"%s%s%s}",
        status ? status : "",
        message ? message : "",
        need_comma ? ",\"data\":" : "",
        data_json ? data_json : "",
        need_comma ? "" : "");
    if (len < 0 || (size_t)len >= sizeof(buf)) return NULL;
    char* out = (char*)malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, buf, len + 1);
    return out;
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
    size_t cap = len * 2 + 3;
    char* out = (char*)malloc(cap);
    if (!out) return NULL;
    char* p = out;
    *p++ = '"';
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)input[i];
        switch (c) {
        case '"':  *p++ = '\\'; *p++ = '"';  break;
        case '\\': *p++ = '\\'; *p++ = '\\'; break;
        case '\n': *p++ = '\\'; *p++ = 'n';  break;
        case '\r': *p++ = '\\'; *p++ = 'r';  break;
        case '\t': *p++ = '\\'; *p++ = 't';  break;
        default:
            if (c < 0x20) {
                p += sprintf(p, "\\u%04x", c);
            } else {
                *p++ = c;
            }
            break;
        }
    }
    *p++ = '"';
    *p = '\0';
    return out;
}

/* ============================================================
 * 解析 API
 * ============================================================ */

typedef struct {
    const char* s;
    size_t pos;
} Parser;

static void skip_ws(Parser* p)
{
    while (p->s[p->pos] == ' ' || p->s[p->pos] == '\t' ||
           p->s[p->pos] == '\n' || p->s[p->pos] == '\r')
        p->pos++;
}

static JsonValue* parse_value(Parser* p);

static char* parse_string_raw(Parser* p)
{
    if (p->s[p->pos] != '"') return NULL;
    p->pos++;
    size_t cap = 256;
    size_t len = 0;
    char* out = (char*)malloc(cap);
    if (!out) return NULL;
    while (p->s[p->pos] && p->s[p->pos] != '"') {
        char c = p->s[p->pos];
        if (c == '\\') {
            p->pos++;
            char esc = p->s[p->pos];
            switch (esc) {
            case '"':  c = '"';  break;
            case '\\': c = '\\'; break;
            case '/':  c = '/';  break;
            case 'n':  c = '\n'; break;
            case 'r':  c = '\r'; break;
            case 't':  c = '\t'; break;
            case 'u': {
                unsigned cp = 0;
                for (int i = 0; i < 4; i++) {
                    p->pos++;
                    char h = p->s[p->pos];
                    cp <<= 4;
                    if (h >= '0' && h <= '9') cp |= (h - '0');
                    else if (h >= 'a' && h <= 'f') cp |= (h - 'a' + 10);
                    else if (h >= 'A' && h <= 'F') cp |= (h - 'A' + 10);
                }
                if (len + 4 >= cap) {
                    cap *= 2;
                    out = (char*)realloc(out, cap);
                    if (!out) return NULL;
                }
                if (cp <= 0x7F) {
                    out[len++] = (char)cp;
                } else if (cp <= 0x7FF) {
                    out[len++] = (char)(0xC0 | (cp >> 6));
                    out[len++] = (char)(0x80 | (cp & 0x3F));
                } else {
                    out[len++] = (char)(0xE0 | (cp >> 12));
                    out[len++] = (char)(0x80 | ((cp >> 6) & 0x3F));
                    out[len++] = (char)(0x80 | (cp & 0x3F));
                }
                p->pos++;
                continue;
            }
            default: break;
            }
        }
        if (len + 1 >= cap) {
            cap *= 2;
            out = (char*)realloc(out, cap);
            if (!out) return NULL;
        }
        out[len++] = c;
        p->pos++;
    }
    if (p->s[p->pos] == '"') p->pos++;
    out[len] = '\0';
    return out;
}

static JsonValue* parse_array(Parser* p)
{
    p->pos++; // skip '['
    JsonValue* val = (JsonValue*)calloc(1, sizeof(JsonValue));
    if (!val) return NULL;
    val->type = JSON_ARRAY;
    size_t cap = 4;
    val->array.items = (JsonValue**)malloc(cap * sizeof(JsonValue*));
    if (!val->array.items) { free(val); return NULL; }
    skip_ws(p);
    if (p->s[p->pos] == ']') { p->pos++; return val; }
    while (1) {
        skip_ws(p);
        JsonValue* item = parse_value(p);
        if (!item) { json_value_free(val); return NULL; }
        if (val->array.count >= cap) {
            cap *= 2;
            val->array.items = (JsonValue**)realloc(val->array.items, cap * sizeof(JsonValue*));
            if (!val->array.items) { json_value_free(val); return NULL; }
        }
        val->array.items[val->array.count++] = item;
        skip_ws(p);
        if (p->s[p->pos] == ']') { p->pos++; break; }
        if (p->s[p->pos] != ',') { json_value_free(val); return NULL; }
        p->pos++;
    }
    return val;
}

static JsonValue* parse_object(Parser* p)
{
    p->pos++; // skip '{'
    JsonValue* val = (JsonValue*)calloc(1, sizeof(JsonValue));
    if (!val) return NULL;
    val->type = JSON_OBJECT;
    size_t cap = 4;
    val->object.pairs = (JsonPair*)malloc(cap * sizeof(JsonPair));
    if (!val->object.pairs) { free(val); return NULL; }
    skip_ws(p);
    if (p->s[p->pos] == '}') { p->pos++; return val; }
    while (1) {
        skip_ws(p);
        char* key = parse_string_raw(p);
        if (!key) { json_value_free(val); return NULL; }
        skip_ws(p);
        if (p->s[p->pos] != ':') { free(key); json_value_free(val); return NULL; }
        p->pos++;
        skip_ws(p);
        JsonValue* item = parse_value(p);
        if (!item) { free(key); json_value_free(val); return NULL; }
        if (val->object.count >= cap) {
            cap *= 2;
            val->object.pairs = (JsonPair*)realloc(val->object.pairs, cap * sizeof(JsonPair));
            if (!val->object.pairs) { free(key); json_value_free(val); return NULL; }
        }
        val->object.pairs[val->object.count].key = key;
        val->object.pairs[val->object.count].value = item;
        val->object.count++;
        skip_ws(p);
        if (p->s[p->pos] == '}') { p->pos++; break; }
        if (p->s[p->pos] != ',') { json_value_free(val); return NULL; }
        p->pos++;
    }
    return val;
}

static JsonValue* parse_value(Parser* p)
{
    skip_ws(p);
    char c = p->s[p->pos];
    if (c == '\0') return NULL;

    if (c == '"') {
        JsonValue* val = (JsonValue*)calloc(1, sizeof(JsonValue));
        if (!val) return NULL;
        val->type = JSON_STRING;
        val->string_val = parse_string_raw(p);
        if (!val->string_val) { free(val); return NULL; }
        return val;
    }
    if (c == '{') return parse_object(p);
    if (c == '[') return parse_array(p);
    if (c == 't' && strncmp(p->s + p->pos, "true", 4) == 0) {
        p->pos += 4;
        JsonValue* val = (JsonValue*)calloc(1, sizeof(JsonValue));
        if (!val) return NULL;
        val->type = JSON_BOOL;
        val->bool_val = 1;
        return val;
    }
    if (c == 'f' && strncmp(p->s + p->pos, "false", 5) == 0) {
        p->pos += 5;
        JsonValue* val = (JsonValue*)calloc(1, sizeof(JsonValue));
        if (!val) return NULL;
        val->type = JSON_BOOL;
        val->bool_val = 0;
        return val;
    }
    if (c == 'n' && strncmp(p->s + p->pos, "null", 4) == 0) {
        p->pos += 4;
        JsonValue* val = (JsonValue*)calloc(1, sizeof(JsonValue));
        if (!val) return NULL;
        val->type = JSON_NULL;
        return val;
    }
    if (c == '-' || (c >= '0' && c <= '9')) {
        char* end = NULL;
        double num = strtod(p->s + p->pos, &end);
        if (end == p->s + p->pos) return NULL;
        p->pos = end - p->s;
        JsonValue* val = (JsonValue*)calloc(1, sizeof(JsonValue));
        if (!val) return NULL;
        val->type = JSON_NUMBER;
        val->number_val = num;
        return val;
    }
    return NULL;
}

JsonValue* json_parse(const char* input)
{
    if (!input) return NULL;
    Parser p = { input, 0 };
    JsonValue* val = parse_value(&p);
    return val;
}

static void json_value_free_internal(JsonValue* val)
{
    if (!val) return;
    switch (val->type) {
    case JSON_STRING:
        free(val->string_val);
        break;
    case JSON_ARRAY:
        for (size_t i = 0; i < val->array.count; i++)
            json_value_free_internal(val->array.items[i]);
        free(val->array.items);
        break;
    case JSON_OBJECT:
        for (size_t i = 0; i < val->object.count; i++) {
            free(val->object.pairs[i].key);
            json_value_free_internal(val->object.pairs[i].value);
        }
        free(val->object.pairs);
        break;
    default:
        break;
    }
    free(val);
}

void json_value_free(JsonValue* val)
{
    json_value_free_internal(val);
}

/* ============================================================
 * 查询 API
 * ============================================================ */

JsonValue* json_object_get(const JsonValue* val, const char* key)
{
    if (!val || val->type != JSON_OBJECT || !key) return NULL;
    for (size_t i = 0; i < val->object.count; i++) {
        if (strcmp(val->object.pairs[i].key, key) == 0)
            return val->object.pairs[i].value;
    }
    return NULL;
}

JsonValue* json_array_get(const JsonValue* val, size_t index)
{
    if (!val || val->type != JSON_ARRAY || index >= val->array.count) return NULL;
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
 * 便利函数
 * ============================================================ */

char* json_extract_string(const char* json_str, const char* key)
{
    JsonValue* root = json_parse(json_str);
    if (!root) return NULL;
    JsonValue* child = json_object_get(root, key);
    char* result = NULL;
    if (child && child->type == JSON_STRING && child->string_val) {
        result = strdup(child->string_val);
    }
    json_value_free(root);
    return result;
}

double json_extract_number(const char* json_str, const char* key)
{
    JsonValue* root = json_parse(json_str);
    if (!root) return 0.0;
    JsonValue* child = json_object_get(root, key);
    double result = 0.0;
    if (child) {
        if (child->type == JSON_NUMBER)
            result = child->number_val;
        else if (child->type == JSON_STRING && child->string_val)
            result = strtod(child->string_val, NULL);
    }
    json_value_free(root);
    return result;
}
