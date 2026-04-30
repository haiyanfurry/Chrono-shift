/**
 * Chrono-shift 数据库模板模块
 * 语言标准: C99
 *
 * 包含：模板 JSON 构建、模板 CRUD 操作
 */

#include "database.h"
#include "db_core.h"
#include "server.h"
#include "json_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "platform_compat.h"

/* ============================================================
 * 模板 JSON 构建辅助
 * ============================================================ */

static char* build_template_json(int64_t template_id, const char* name,
                                  int64_t author_id, const char* css_path,
                                  const char* preview_url, int64_t downloads,
                                  int64_t created_at)
{
    char* safe_name       = json_escape_string(name       ? name       : "");
    char* safe_css_path   = json_escape_string(css_path   ? css_path   : "");
    char* safe_preview_url = json_escape_string(preview_url ? preview_url : "");

    if (!safe_name || !safe_css_path || !safe_preview_url) {
        free(safe_name); free(safe_css_path); free(safe_preview_url);
        return NULL;
    }

    size_t len = 256 + strlen(safe_name) + strlen(safe_css_path) + strlen(safe_preview_url);
    char* json = (char*)malloc(len);
    if (!json) {
        free(safe_name); free(safe_css_path); free(safe_preview_url);
        return NULL;
    }

    snprintf(json, len,
             "{"
             "\"id\":%lld,"
             "\"name\":\"%s\","
             "\"author_id\":%lld,"
             "\"css_path\":\"%s\","
             "\"preview_url\":\"%s\","
             "\"downloads\":%lld,"
             "\"created_at\":%lld"
             "}",
             (long long)template_id, safe_name, (long long)author_id,
             safe_css_path, safe_preview_url,
             (long long)downloads, (long long)created_at);

    free(safe_name); free(safe_css_path); free(safe_preview_url);
    return json;
}

/* ============================================================
 * 模板 CRUD
 * ============================================================ */

int db_create_template(const char* name, int64_t author_id, const char* css_path,
                       const char* preview_url, int64_t* template_id)
{
    if (!name || !css_path || !template_id) {
        LOG_ERROR("db_create_template: 参数无效");
        return -1;
    }

    int64_t tmpl_id = allocate_id();
    if (tmpl_id < 0) return -1;

    char* json = build_template_json(tmpl_id, name, author_id, css_path,
                                      preview_url ? preview_url : "", 0,
                                      (int64_t)time(NULL));
    if (!json) return -1;

    char path[DB_MAX_PATH];
    get_template_path(tmpl_id, path, sizeof(path));

    FILE* f = fopen(path, "w");
    if (!f) {
        free(json);
        return -1;
    }
    fprintf(f, "%s\n", json);
    fclose(f);
    free(json);

    *template_id = tmpl_id;
    LOG_INFO("模板创建成功: id=%lld, name=%s", (long long)tmpl_id, name);
    return 0;
}

int db_get_templates(int64_t offset, int64_t limit, int64_t* ids, char** names,
                     int64_t* author_ids, char** preview_urls, int64_t* downloads,
                     size_t* count)
{
    if (!ids || !names || !author_ids || !preview_urls || !downloads || !count) return -1;
    *count = 0;

    char tmpl_dir[DB_MAX_PATH];
    get_templates_dir(tmpl_dir, sizeof(tmpl_dir));

    /* 临时存储模板条目用于排序 */
    typedef struct {
        int64_t id;
        int64_t created_at;
        int64_t dl_count;
        char* name;
        int64_t author_id;
        char* preview_url;
    } TmplEntry;

    TmplEntry entries[1024];
    size_t total = 0;

    DirIterator it;
    if (dir_open(&it, tmpl_dir) != 0) return 0;

    char name_buffer[256];
    while (total < 1024 && dir_next(&it, name_buffer, sizeof(name_buffer)) == 0) {
        size_t len = strlen(name_buffer);
        if (len < 6 || strcmp(name_buffer + len - 5, ".json") != 0) continue;

        char filepath[DB_MAX_PATH];
        snprintf(filepath, sizeof(filepath), "%s/%s", tmpl_dir, name_buffer);

        char* content = read_file_content(filepath);
        if (!content) continue;

        JsonValue* root = json_parse(content);
        free(content);
        if (!root) continue;

        JsonValue* v;
        v = json_object_get(root, "id");
        if (!v || v->type != JSON_NUMBER) { json_value_free(root); continue; }
        entries[total].id = (int64_t)v->number_val;

        v = json_object_get(root, "name");
        entries[total].name = (v && v->type == JSON_STRING) ? strdup(v->string_val) : strdup("");

        v = json_object_get(root, "author_id");
        entries[total].author_id = (v && v->type == JSON_NUMBER) ? (int64_t)v->number_val : 0;

        v = json_object_get(root, "preview_url");
        entries[total].preview_url = (v && v->type == JSON_STRING) ? strdup(v->string_val) : strdup("");

        v = json_object_get(root, "downloads");
        entries[total].dl_count = (v && v->type == JSON_NUMBER) ? (int64_t)v->number_val : 0;

        v = json_object_get(root, "created_at");
        entries[total].created_at = (v && v->type == JSON_NUMBER) ? (int64_t)v->number_val : 0;

        total++;
        json_value_free(root);
    }
    dir_close(&it);

    /* 按创建时间降序排序 */
    for (size_t i = 0; i < total; i++) {
        for (size_t j = i + 1; j < total; j++) {
            if (entries[j].created_at > entries[i].created_at) {
                TmplEntry tmp = entries[i];
                entries[i] = entries[j];
                entries[j] = tmp;
            }
        }
    }

    /* 应用 offset/limit */
    size_t start = ((size_t)offset < total) ? (size_t)offset : total;
    size_t end = (start + (size_t)limit < total) ? start + (size_t)limit : total;

    size_t idx = 0;
    for (size_t i = start; i < end; i++, idx++) {
        ids[idx]          = entries[i].id;
        names[idx]        = entries[i].name ? entries[i].name : strdup("");
        author_ids[idx]   = entries[i].author_id;
        preview_urls[idx] = entries[i].preview_url ? entries[i].preview_url : strdup("");
        downloads[idx]    = entries[i].dl_count;

        /* 释放原指针防止后续泄漏 (已转移所有权) */
        entries[i].name = NULL;
        entries[i].preview_url = NULL;
    }

    /* 释放未使用的条目 */
    for (size_t i = start; i < total; i++) {
        free(entries[i].name);
        free(entries[i].preview_url);
    }

    *count = idx;
    return 0;
}

int db_apply_template(int64_t user_id, int64_t template_id)
{
    if (user_id <= 0 || template_id <= 0) return -1;

    /* 写入用户模板配置 */
    char path[DB_MAX_PATH];
    snprintf(path, sizeof(path), "%s/%s/%lld_template.json",
             g_db_base, "templates", (long long)user_id);

    FILE* f = fopen(path, "w");
    if (!f) return -1;

    fprintf(f, "{\"user_id\":%lld,\"template_id\":%lld,\"applied_at\":%lld}\n",
            (long long)user_id, (long long)template_id, (long long)time(NULL));
    fclose(f);

    LOG_INFO("用户 %lld 应用了模板 %lld", (long long)user_id, (long long)template_id);
    return 0;
}

int db_get_user_template(int64_t user_id, int64_t* template_id)
{
    if (!template_id) return -1;
    *template_id = 0;

    char path[DB_MAX_PATH];
    snprintf(path, sizeof(path), "%s/%s/%lld_template.json",
             g_db_base, "templates", (long long)user_id);

    char* content = read_file_content(path);
    if (!content) return -1;

    double val = json_extract_number(content, "template_id");
    free(content);

    *template_id = (int64_t)val;
    return (*template_id > 0) ? 0 : -1;
}

int db_increment_template_downloads(int64_t template_id)
{
    char path[DB_MAX_PATH];
    get_template_path(template_id, path, sizeof(path));

    char* content = read_file_content(path);
    if (!content) return -1;

    JsonValue* root = json_parse(content);
    free(content);
    if (!root) return -1;

    JsonValue* v = json_object_get(root, "downloads");
    int64_t dl = (v && v->type == JSON_NUMBER) ? (int64_t)v->number_val + 1 : 1;

    /* 提取各字段 */
    v = json_object_get(root, "name");         char* name = v && v->type == JSON_STRING ? strdup(v->string_val) : strdup("");
    v = json_object_get(root, "author_id");    int64_t author = v && v->type == JSON_NUMBER ? (int64_t)v->number_val : 0;
    v = json_object_get(root, "css_path");     char* css = v && v->type == JSON_STRING ? strdup(v->string_val) : strdup("");
    v = json_object_get(root, "preview_url");  char* prev = v && v->type == JSON_STRING ? strdup(v->string_val) : strdup("");
    v = json_object_get(root, "created_at");   int64_t created = v && v->type == JSON_NUMBER ? (int64_t)v->number_val : 0;
    json_value_free(root);

    char* new_json = build_template_json(template_id, name, author, css, prev, dl, created);
    free(name); free(css); free(prev);

    if (!new_json) return -1;

    FILE* f = fopen(path, "w");
    if (!f) { free(new_json); return -1; }
    fprintf(f, "%s\n", new_json);
    fclose(f);
    free(new_json);

    return 0;
}
