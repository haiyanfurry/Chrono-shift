/**
 * Chrono-shift 社区/模板管理 HTTP 处理器
 * 语言标准: C99
 *
 * 实现模板列表、上传、下载、应用、预览
 * CSS 文件存储路径: data/db/chrono.db/templates/{template_id}.css
 * 模板元数据: data/db/chrono.db/templates/{template_id}.json
 */

#include "community_handler.h"
#include "database.h"
#include "json_parser.h"
#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ============================================================
 * Rust FFI 函数声明
 * ============================================================ */

extern char*  rust_verify_jwt(const char* token);
extern void   rust_free_string(char* s);

/* ============================================================
 * 辅助函数
 * ============================================================ */

/* 从请求 body 中提取 JSON 字符串字段 */
static char* get_json_string_field(const uint8_t* body, size_t body_len, const char* key)
{
    if (!body || body_len == 0) return NULL;

    char* body_str = (char*)malloc(body_len + 1);
    if (!body_str) return NULL;
    memcpy(body_str, body, body_len);
    body_str[body_len] = '\0';

    JsonValue* root = json_parse(body_str);
    free(body_str);

    if (!root) return NULL;

    JsonValue* val = json_object_get(root, key);
    char* result = NULL;
    if (val && val->type == JSON_STRING) {
        result = strdup(val->string_val);
    }

    json_value_free(root);
    return result;
}

/* 从请求 body 中提取 JSON 数值字段 */
static int64_t get_json_number_field(const uint8_t* body, size_t body_len, const char* key)
{
    if (!body || body_len == 0) return 0;

    char* body_str = (char*)malloc(body_len + 1);
    if (!body_str) return 0;
    memcpy(body_str, body, body_len);
    body_str[body_len] = '\0';

    JsonValue* root = json_parse(body_str);
    free(body_str);

    if (!root) return 0;

    JsonValue* val = json_object_get(root, key);
    int64_t result = (val && val->type == JSON_NUMBER) ? (int64_t)val->number_val : 0;

    json_value_free(root);
    return result;
}

/* 从查询字符串中提取数值参数 */
static int64_t get_query_int(const char* query, const char* key)
{
    if (!query || !key) return 0;

    size_t key_len = strlen(key);
    const char* p = query;
    while ((p = strstr(p, key)) != NULL) {
        if ((p == query || *(p - 1) == '&' || *(p - 1) == '?') &&
            p[key_len] == '=') {
            return (int64_t)atoll(p + key_len + 1);
        }
        p++;
    }
    return 0;
}

/* 从 Authorization header 提取并验证 JWT, 返回 user_id */
static int64_t authenticate_request(const HttpRequest* req)
{
    const char* token = http_extract_bearer_token(req->headers, req->header_count);
    if (!token) return 0;

    char* uid_str = rust_verify_jwt(token);
    if (!uid_str) return 0;

    int64_t user_id = (int64_t)atoll(uid_str);
    rust_free_string(uid_str);
    return user_id;
}

/* 获取模板 CSS 文件路径 */
static void get_template_css_path(int64_t template_id, char* path, size_t path_size)
{
    snprintf(path, path_size, "data/db/chrono.db/templates/%lld.css",
             (long long)template_id);
}

/* 读取文件内容到字符串 */
static char* read_file_content(const char* path)
{
    FILE* f = fopen(path, "r");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0) {
        fclose(f);
        return NULL;
    }

    char* content = (char*)malloc((size_t)size + 1);
    if (!content) {
        fclose(f);
        return NULL;
    }

    size_t read_size = fread(content, 1, (size_t)size, f);
    fclose(f);
    content[read_size] = '\0';

    return content;
}

/* 写入文件内容 */
static int write_file_content(const char* path, const char* content)
{
    FILE* f = fopen(path, "w");
    if (!f) return -1;

    size_t len = strlen(content);
    size_t written = fwrite(content, 1, len, f);
    fclose(f);

    return (written == len) ? 0 : -1;
}

/* 检查路径是否包含路径遍历攻击 (../ 或 ..\) */
static int has_path_traversal(const char* path)
{
    return (strstr(path, "..") != NULL ||
            strstr(path, "/") != NULL ||
            strstr(path, "\\") != NULL);
}

/* ============================================================
 * HTTP API 实现
 * ============================================================ */

/**
 * GET /api/templates?offset=0&limit=20
 * Auth: Bearer JWT
 * 返回模板列表，按创建时间倒序
 */
void handle_template_list(const HttpRequest* req, HttpResponse* resp, void* user_data)
{
    (void)user_data;
    http_response_init(resp);

    /* 认证 */
    int64_t user_id = authenticate_request(req);
    if (user_id <= 0) {
        http_response_set_json(resp, json_build_error("未授权，请先登录"));
        http_response_set_status(resp, 401, "Unauthorized");
        return;
    }

    /* 解析查询参数 */
    int64_t offset = get_query_int(req->query, "offset");
    int64_t limit  = get_query_int(req->query, "limit");

    if (offset < 0) offset = 0;
    if (limit <= 0 || limit > 50) limit = 20;

    /* 分配缓冲区 */
    int64_t* ids          = (int64_t*)malloc((size_t)limit * sizeof(int64_t));
    int64_t* author_ids  = (int64_t*)malloc((size_t)limit * sizeof(int64_t));
    char**   names        = (char**)calloc((size_t)limit, sizeof(char*));
    char**   preview_urls = (char**)calloc((size_t)limit, sizeof(char*));
    int64_t* downloads    = (int64_t*)malloc((size_t)limit * sizeof(int64_t));

    if (!ids || !author_ids || !names || !preview_urls || !downloads) {
        free(ids); free(author_ids); free(names); free(preview_urls); free(downloads);
        http_response_set_json(resp, json_build_error("内存不足"));
        http_response_set_status(resp, 500, "Internal Server Error");
        return;
    }

    size_t count = 0;
    int ret = db_get_templates(offset, limit, ids, names, author_ids,
                                preview_urls, downloads, &count);

    if (ret != 0) {
        free(ids); free(author_ids); free(names); free(preview_urls); free(downloads);
        http_response_set_json(resp, json_build_error("获取模板列表失败"));
        http_response_set_status(resp, 500, "Internal Server Error");
        return;
    }

    /* 构建 JSON 数组 */
    size_t total_len = 64 + count * 256;
    char* arr_json = (char*)malloc(total_len);
    if (!arr_json) {
        for (size_t i = 0; i < count; i++) { free(names[i]); free(preview_urls[i]); }
        free(ids); free(author_ids); free(names); free(preview_urls); free(downloads);
        http_response_set_json(resp, json_build_error("内存不足"));
        http_response_set_status(resp, 500, "Internal Server Error");
        return;
    }

    size_t pos = 0;
    pos += snprintf(arr_json + pos, total_len - pos, "[");

    for (size_t i = 0; i < count; i++) {
        if (i > 0 && pos < total_len - 2) {
            arr_json[pos++] = ',';
        }

        /* 获取作者名称 */
        char* author_name = NULL;
        char* auth_nick = NULL;
        char* auth_av = NULL;
        if (db_get_user_by_id(author_ids[i], &author_name, &auth_nick, &auth_av) != 0) {
            author_name = strdup("未知");
        }
        char* safe_name = json_escape_string(names[i] ? names[i] : "");
        char* safe_author = json_escape_string(auth_nick ? auth_nick :
                                                (author_name ? author_name : "未知"));
        char* safe_preview = json_escape_string(preview_urls[i] ? preview_urls[i] : "");

        int n = snprintf(arr_json + pos, total_len - pos,
                         "{"
                         "\"id\":%lld,"
                         "\"name\":\"%s\","
                         "\"author_id\":%lld,"
                         "\"author_name\":\"%s\","
                         "\"preview_url\":\"%s\","
                         "\"downloads\":%lld"
                         "}",
                         (long long)ids[i],
                         safe_name ? safe_name : "",
                         (long long)author_ids[i],
                         safe_author ? safe_author : "",
                         safe_preview ? safe_preview : "",
                         (long long)downloads[i]);

        if (n > 0) pos += (size_t)n;

        free(safe_name);
        free(safe_author);
        free(safe_preview);
        if (author_name) free(author_name);
        if (auth_nick) free(auth_nick);
        if (auth_av) free(auth_av);

        free(names[i]);
        free(preview_urls[i]);
    }

    if (pos < total_len) {
        arr_json[pos] = '\0';
    }
    arr_json[total_len - 1] = '\0';

    /* 构建最终响应 */
    char* data_json = NULL;
    size_t data_len = strlen(arr_json) + 32;
    data_json = (char*)malloc(data_len);
    if (data_json) {
        snprintf(data_json, data_len, "{\"templates\":%s}", arr_json);
    }

    char* response = json_build_success(data_json ? data_json : "{\"templates\":[]}");
    http_response_set_json(resp, response);

    free(response);
    free(data_json);
    free(arr_json);
    free(ids);
    free(author_ids);
    free(names);
    free(preview_urls);
    free(downloads);
}

/**
 * POST /api/templates/upload
 * Auth: Bearer JWT
 * Body: { "name": string, "css_content": string }
 * 上传模板 (CSS 文件 + 元数据)
 */
void handle_template_upload(const HttpRequest* req, HttpResponse* resp, void* user_data)
{
    (void)user_data;
    http_response_init(resp);

    /* 认证 */
    int64_t user_id = authenticate_request(req);
    if (user_id <= 0) {
        http_response_set_json(resp, json_build_error("未授权，请先登录"));
        http_response_set_status(resp, 401, "Unauthorized");
        return;
    }

    /* 解析请求参数 */
    char* name = get_json_string_field(req->body, req->body_length, "name");
    char* css_content = get_json_string_field(req->body, req->body_length, "css_content");

    if (!name || strlen(name) == 0) {
        http_response_set_json(resp, json_build_error("模板名称不能为空"));
        free(name); free(css_content);
        http_response_set_status(resp, 400, "Bad Request");
        return;
    }

    if (!css_content || strlen(css_content) == 0) {
        http_response_set_json(resp, json_build_error("CSS 内容不能为空"));
        free(name); free(css_content);
        http_response_set_status(resp, 400, "Bad Request");
        return;
    }

    /* 检查路径遍历攻击 */
    if (has_path_traversal(name)) {
        http_response_set_json(resp, json_build_error("模板名称包含非法字符"));
        free(name); free(css_content);
        http_response_set_status(resp, 400, "Bad Request");
        return;
    }

    /* 创建模板数据库记录 */
    int64_t template_id = 0;

    /* CSS 文件路径 (在创建模板后确定 ID，先占位) */
    char css_path[256];
    /* 先创建模板以获取 ID */
    char tmp_css_path[64];
    snprintf(tmp_css_path, sizeof(tmp_css_path), "templates/tmp_%lld.css", (long long)time(NULL));

    int ret = db_create_template(name, user_id, tmp_css_path, "", &template_id);
    if (ret != 0 || template_id <= 0) {
        http_response_set_json(resp, json_build_error("模板创建失败"));
        free(name); free(css_content);
        http_response_set_status(resp, 500, "Internal Server Error");
        return;
    }

    /* 保存 CSS 文件 */
    get_template_css_path(template_id, css_path, sizeof(css_path));
    if (write_file_content(css_path, css_content) != 0) {
        http_response_set_json(resp, json_build_error("CSS 文件保存失败"));
        free(name); free(css_content);
        http_response_set_status(resp, 500, "Internal Server Error");
        return;
    }

    /* 构建响应 */
    char data_buf[256];
    snprintf(data_buf, sizeof(data_buf),
             "{\"template_id\":%lld,\"name\":\"%s\"}",
             (long long)template_id, name);

    char* response = json_build_success(data_buf);
    http_response_set_json(resp, response);
    free(response);

    LOG_INFO("模板已上传: id=%lld, name=%s, user=%lld",
             (long long)template_id, name, (long long)user_id);

    free(name);
    free(css_content);
}

/**
 * GET /api/templates/download?id=X
 * 无需认证 (可用于预览)
 * 返回模板 CSS 内容
 */
void handle_template_download(const HttpRequest* req, HttpResponse* resp, void* user_data)
{
    (void)user_data;
    http_response_init(resp);

    /* 解析模板 ID */
    int64_t template_id = get_query_int(req->query, "id");
    if (template_id <= 0) {
        http_response_set_json(resp, json_build_error("请指定模板 ID"));
        http_response_set_status(resp, 400, "Bad Request");
        return;
    }

    /* 读取 CSS 文件 */
    char css_path[256];
    get_template_css_path(template_id, css_path, sizeof(css_path));

    char* css_content = read_file_content(css_path);
    if (!css_content) {
        http_response_set_json(resp, json_build_error("模板文件不存在"));
        http_response_set_status(resp, 404, "Not Found");
        return;
    }

    /* 增加下载计数 (忽略错误) */
    db_increment_template_downloads(template_id);

    /* 返回 CSS 内容 (text/plain) */
    http_response_set_header(resp, "Content-Type", "text/plain; charset=utf-8");
    http_response_set_body(resp, (const uint8_t*)css_content, strlen(css_content));

    free(css_content);
}

/**
 * POST /api/templates/apply
 * Auth: Bearer JWT
 * Body: { "template_id": number }
 * 应用模板到当前用户
 */
void handle_template_apply(const HttpRequest* req, HttpResponse* resp, void* user_data)
{
    (void)user_data;
    http_response_init(resp);

    /* 认证 */
    int64_t user_id = authenticate_request(req);
    if (user_id <= 0) {
        http_response_set_json(resp, json_build_error("未授权，请先登录"));
        http_response_set_status(resp, 401, "Unauthorized");
        return;
    }

    /* 解析模板 ID */
    int64_t template_id = get_json_number_field(req->body, req->body_length, "template_id");
    if (template_id <= 0) {
        http_response_set_json(resp, json_build_error("请指定模板 ID"));
        http_response_set_status(resp, 400, "Bad Request");
        return;
    }

    /* 应用模板 */
    int ret = db_apply_template(user_id, template_id);
    if (ret != 0) {
        http_response_set_json(resp, json_build_error("模板应用失败"));
        http_response_set_status(resp, 500, "Internal Server Error");
        return;
    }

    /* 增加下载计数 */
    db_increment_template_downloads(template_id);

    char data_buf[128];
    snprintf(data_buf, sizeof(data_buf),
             "{\"template_id\":%lld,\"user_id\":%lld}",
             (long long)template_id, (long long)user_id);

    char* response = json_build_success(data_buf);
    http_response_set_json(resp, response);
    free(response);

    LOG_INFO("模板已应用: template_id=%lld, user_id=%lld",
             (long long)template_id, (long long)user_id);
}

/**
 * GET /api/templates/preview?id=X
 * 无需认证
 * 返回模板 CSS 内容 (与 download 相同但不增加计数)
 */
void handle_template_preview(const HttpRequest* req, HttpResponse* resp, void* user_data)
{
    (void)user_data;
    http_response_init(resp);

    /* 解析模板 ID */
    int64_t template_id = get_query_int(req->query, "id");
    if (template_id <= 0) {
        http_response_set_json(resp, json_build_error("请指定模板 ID"));
        http_response_set_status(resp, 400, "Bad Request");
        return;
    }

    /* 读取 CSS 文件 */
    char css_path[256];
    get_template_css_path(template_id, css_path, sizeof(css_path));

    char* css_content = read_file_content(css_path);
    if (!css_content) {
        http_response_set_json(resp, json_build_error("模板文件不存在"));
        http_response_set_status(resp, 404, "Not Found");
        return;
    }

    /* 返回 CSS 内容 (text/plain) */
    http_response_set_header(resp, "Content-Type", "text/plain; charset=utf-8");
    http_response_set_body(resp, (const uint8_t*)css_content, strlen(css_content));

    free(css_content);
}
