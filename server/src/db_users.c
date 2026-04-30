/**
 * Chrono-shift 数据库用户模块
 * 语言标准: C99
 *
 * 包含：用户 JSON 构建、用户 CRUD 操作
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
 * 用户 JSON 构建辅助
 * ============================================================ */

static char* build_user_json(int64_t user_id, const char* username,
                              const char* password_hash, const char* nickname,
                              const char* avatar_url, int64_t created_at)
{
    /* 先转义各字符串字段, 防止 JSON 注入 */
    char* safe_username     = json_escape_string(username     ? username     : "");
    char* safe_password_hash = json_escape_string(password_hash ? password_hash : "");
    char* safe_nickname     = json_escape_string(nickname     ? nickname     : "");
    char* safe_avatar_url   = json_escape_string(avatar_url   ? avatar_url   : "");

    if (!safe_username || !safe_password_hash || !safe_nickname || !safe_avatar_url) {
        free(safe_username); free(safe_password_hash);
        free(safe_nickname); free(safe_avatar_url);
        return NULL;
    }

    /* 计算所需缓冲区大小 */
    size_t len = 128 + strlen(safe_username) + strlen(safe_password_hash)
                 + strlen(safe_nickname) + strlen(safe_avatar_url);

    char* json = (char*)malloc(len);
    if (!json) {
        free(safe_username); free(safe_password_hash);
        free(safe_nickname); free(safe_avatar_url);
        return NULL;
    }

    snprintf(json, len,
             "{"
             "\"id\":%lld,"
             "\"username\":\"%s\","
             "\"password_hash\":\"%s\","
             "\"nickname\":\"%s\","
             "\"avatar_url\":\"%s\","
             "\"created_at\":%lld"
             "}",
             (long long)user_id,
             safe_username,
             safe_password_hash,
             safe_nickname,
             safe_avatar_url,
             (long long)created_at);

    free(safe_username); free(safe_password_hash);
    free(safe_nickname); free(safe_avatar_url);

    return json;
}

/* ============================================================
 * 用户 CRUD
 * ============================================================ */

int db_create_user(const char* username, const char* password_hash,
                   const char* nickname, const char* avatar_url)
{
    if (!username || !password_hash) {
        LOG_ERROR("db_create_user: 参数无效");
        return -1;
    }

    /* 检查用户名是否已存在 */
    int64_t dummy_id;
    char* dummy_hash;
    char* dummy_nick;
    if (db_get_user_by_username(username, &dummy_id, &dummy_hash, &dummy_nick) == 0) {
        free(dummy_hash);
        free(dummy_nick);
        LOG_WARN("用户名已存在: %s", username);
        return -2; /* 用户名已存在 */
    }

    /* 分配新 ID */
    int64_t user_id = allocate_id();
    if (user_id < 0) {
        LOG_ERROR("分配用户 ID 失败");
        return -1;
    }

    /* 生成 JSON */
    char* json = build_user_json(user_id, username, password_hash,
                                  nickname ? nickname : username,
                                  avatar_url ? avatar_url : "",
                                  (int64_t)time(NULL));
    if (!json) {
        LOG_ERROR("构建用户 JSON 失败");
        return -1;
    }

    /* 写入文件 */
    char path[DB_MAX_PATH];
    get_user_path(user_id, path, sizeof(path));

    FILE* f = fopen(path, "w");
    if (!f) {
        LOG_ERROR("写入用户文件失败: %s", path);
        free(json);
        return -1;
    }

    fprintf(f, "%s\n", json);
    fclose(f);
    free(json);

    LOG_INFO("用户创建成功: id=%lld, username=%s", (long long)user_id, username);
    return (int)user_id;
}

int db_get_user_by_id(int64_t user_id, char** username, char** nickname, char** avatar_url)
{
    if (!username || !nickname || !avatar_url) return -1;

    char path[DB_MAX_PATH];
    get_user_path(user_id, path, sizeof(path));

    char* content = read_file_content(path);
    if (!content) {
        LOG_DEBUG("用户不存在: %lld", (long long)user_id);
        return -1;
    }

    JsonValue* root = json_parse(content);
    free(content);

    if (!root) {
        LOG_ERROR("解析用户 JSON 失败: %lld", (long long)user_id);
        return -1;
    }

    JsonValue* v;

    v = json_object_get(root, "username");
    *username = v && v->type == JSON_STRING ? strdup(v->string_val) : strdup("");

    v = json_object_get(root, "nickname");
    *nickname = v && v->type == JSON_STRING ? strdup(v->string_val) : strdup("");

    v = json_object_get(root, "avatar_url");
    *avatar_url = v && v->type == JSON_STRING ? strdup(v->string_val) : strdup("");

    json_value_free(root);
    return 0;
}

int db_get_user_by_username(const char* username, int64_t* user_id,
                             char** password_hash, char** nickname)
{
    if (!username || !user_id || !password_hash || !nickname) return -1;

    char users_dir[DB_MAX_PATH];
    get_users_dir(users_dir, sizeof(users_dir));

    /* 使用跨平台 DirIterator 遍历目录 */
    DirIterator it;
    if (dir_open(&it, users_dir) != 0) return -1;

    int found = 0;
    char name_buffer[256];

    while (dir_next(&it, name_buffer, sizeof(name_buffer)) == 0) {
        /* 只处理 .json 文件 */
        size_t len = strlen(name_buffer);
        if (len < 6 || strcmp(name_buffer + len - 5, ".json") != 0) continue;

        char filepath[DB_MAX_PATH];
        snprintf(filepath, sizeof(filepath), "%s/%s", users_dir, name_buffer);

        char* content = read_file_content(filepath);
        if (!content) continue;

        JsonValue* root = json_parse(content);
        free(content);

        if (!root) continue;

        JsonValue* v = json_object_get(root, "username");
        if (v && v->type == JSON_STRING && strcmp(v->string_val, username) == 0) {
            /* 找到匹配的用户 */
            v = json_object_get(root, "id");
            if (v && v->type == JSON_NUMBER) {
                *user_id = (int64_t)v->number_val;
            }

            v = json_object_get(root, "password_hash");
            *password_hash = v && v->type == JSON_STRING ? strdup(v->string_val) : strdup("");

            v = json_object_get(root, "nickname");
            *nickname = v && v->type == JSON_STRING ? strdup(v->string_val) : strdup("");

            found = 1;
        }

        json_value_free(root);
        if (found) break;
    }

    dir_close(&it);
    return found ? 0 : -1;
}

int db_update_user_profile(int64_t user_id, const char* nickname, const char* avatar_url)
{
    char path[DB_MAX_PATH];
    get_user_path(user_id, path, sizeof(path));

    /* 读取现有数据 */
    char* content = read_file_content(path);
    if (!content) {
        LOG_ERROR("更新用户资料: 用户不存在 %lld", (long long)user_id);
        return -1;
    }

    JsonValue* root = json_parse(content);
    free(content);

    if (!root) return -1;

    /* 提取现有字段 */
    JsonValue* v;
    char* username = NULL;
    char* password_hash = NULL;
    char* old_nickname = NULL;
    char* old_avatar = NULL;
    int64_t created_at = 0;

    v = json_object_get(root, "username");
    if (v && v->type == JSON_STRING) username = strdup(v->string_val);

    v = json_object_get(root, "password_hash");
    if (v && v->type == JSON_STRING) password_hash = strdup(v->string_val);

    v = json_object_get(root, "nickname");
    if (v && v->type == JSON_STRING) old_nickname = strdup(v->string_val);

    v = json_object_get(root, "avatar_url");
    if (v && v->type == JSON_STRING) old_avatar = strdup(v->string_val);

    v = json_object_get(root, "created_at");
    if (v && v->type == JSON_NUMBER) created_at = (int64_t)v->number_val;

    json_value_free(root);

    /* 构建新 JSON */
    char* new_json = build_user_json(
        user_id,
        username ? username : "",
        password_hash ? password_hash : "",
        nickname ? nickname : (old_nickname ? old_nickname : ""),
        avatar_url ? avatar_url : (old_avatar ? old_avatar : ""),
        created_at);

    free(username);
    free(password_hash);
    free(old_nickname);
    free(old_avatar);

    if (!new_json) return -1;

    /* 写入文件 */
    FILE* f = fopen(path, "w");
    if (!f) {
        free(new_json);
        return -1;
    }
    fprintf(f, "%s\n", new_json);
    fclose(f);
    free(new_json);

    LOG_INFO("用户资料更新成功: %lld", (long long)user_id);
    return 0;
}

int db_search_users(const char* keyword, int64_t* results, size_t max_results, size_t* count)
{
    if (!keyword || !results || !count) return -1;

    *count = 0;

    char users_dir[DB_MAX_PATH];
    get_users_dir(users_dir, sizeof(users_dir));

    /* 使用跨平台 DirIterator 遍历目录 */
    DirIterator it;
    if (dir_open(&it, users_dir) != 0) return 0;

    char name_buffer[256];

    while (*count < max_results && dir_next(&it, name_buffer, sizeof(name_buffer)) == 0) {
        /* 只处理 .json 文件 */
        size_t len = strlen(name_buffer);
        if (len < 6 || strcmp(name_buffer + len - 5, ".json") != 0) continue;

        char filepath[DB_MAX_PATH];
        snprintf(filepath, sizeof(filepath), "%s/%s", users_dir, name_buffer);

        char* content = read_file_content(filepath);
        if (content) {
            JsonValue* root = json_parse(content);
            free(content);

            if (root) {
                JsonValue* v_username = json_object_get(root, "username");
                JsonValue* v_nickname = json_object_get(root, "nickname");

                int match = 0;
                if (v_username && v_username->type == JSON_STRING &&
                    strstr(v_username->string_val, keyword)) {
                    match = 1;
                }
                if (!match && v_nickname && v_nickname->type == JSON_STRING &&
                    strstr(v_nickname->string_val, keyword)) {
                    match = 1;
                }

                if (match) {
                    JsonValue* v_id = json_object_get(root, "id");
                    if (v_id && v_id->type == JSON_NUMBER) {
                        results[*count] = (int64_t)v_id->number_val;
                        (*count)++;
                    }
                }

                json_value_free(root);
            }
        }
    }

    dir_close(&it);
    return 0;
}
