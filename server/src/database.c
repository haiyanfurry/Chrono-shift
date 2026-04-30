/**
 * Chrono-shift 数据库操作
 * 语言标准: C99
 *
 * 开发阶段使用文件型 JSON 存储。
 * 每个用户存储为 data/db/users/{user_id}.json
 * 计数器文件: data/db/next_id.txt
 *
 * 后续可替换为 SQLite3 实现（接口不变）
 */

#include "database.h"
#include "server.h"
#include "json_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "platform_compat.h"
#include <sys/stat.h>

/* ============================================================
 * 内部常量
 * ============================================================ */

#define DB_DIR_MODE  0755
#define USER_DIR     "users"
#define NEXT_ID_FILE "next_id.txt"
#define MAX_PATH     1024

/* ============================================================
 * 文件路径构造
 * ============================================================ */

static char g_db_base[MAX_PATH] = {0};

/* 构建用户文件路径 */
static void get_user_path(int64_t user_id, char* path, size_t path_size)
{
    snprintf(path, path_size, "%s/%s/%lld.json",
             g_db_base, USER_DIR, (long long)user_id);
}

/* 构建用户目录路径 */
static void get_users_dir(char* path, size_t path_size)
{
    snprintf(path, path_size, "%s/%s", g_db_base, USER_DIR);
}

/* 获取 next_id 文件路径 */
static void get_next_id_path(char* path, size_t path_size)
{
    snprintf(path, path_size, "%s/%s", g_db_base, NEXT_ID_FILE);
}

/* ============================================================
 * 文件 I/O 辅助
 * ============================================================ */

static int64_t read_next_id(void)
{
    char path[MAX_PATH];
    get_next_id_path(path, sizeof(path));

    FILE* f = fopen(path, "r");
    if (!f) return 1; /* 从 1 开始 */

    int64_t id = 0;
    if (fscanf(f, "%lld", (long long*)&id) != 1) id = 1;
    fclose(f);
    return id;
}

static int write_next_id(int64_t id)
{
    char path[MAX_PATH];
    get_next_id_path(path, sizeof(path));

    FILE* f = fopen(path, "w");
    if (!f) return -1;

    fprintf(f, "%lld", (long long)id);
    fclose(f);
    return 0;
}

static int64_t allocate_id(void)
{
    int64_t id = read_next_id();
    if (write_next_id(id + 1) != 0) return -1;
    return id;
}

/* 读取 JSON 文件内容 */
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

    size_t n = fread(content, 1, (size_t)size, f);
    content[n] = '\0';
    fclose(f);
    return content;
}

/* 检查文件是否存在 */
static int file_exists(const char* path)
{
    FILE* f = fopen(path, "r");
    if (f) {
        fclose(f);
        return 1;
    }
    return 0;
}

/* ============================================================
 * 目录创建
 * ============================================================ */

static int ensure_dir(const char* path)
{
    struct stat st = {0};
    if (stat(path, &st) == -1) {
#ifdef PLATFORM_WINDOWS
        return _mkdir(path);
#else
        return mkdir(path, DB_DIR_MODE);
#endif
    }
    return 0;
}

/* ============================================================
 * 用户 JSON 构建/解析辅助
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
 * API 实现
 * ============================================================ */

int db_init(const char* db_path)
{
    LOG_INFO("初始化数据库: %s", db_path);

    strncpy(g_db_base, db_path, sizeof(g_db_base) - 1);

    /* 创建目录结构 */
    if (ensure_dir(g_db_base) != 0) {
        LOG_ERROR("创建数据库目录失败: %s", g_db_base);
        return -1;
    }

    char users_dir[MAX_PATH];
    get_users_dir(users_dir, sizeof(users_dir));
    if (ensure_dir(users_dir) != 0) {
        LOG_ERROR("创建用户目录失败: %s", users_dir);
        return -1;
    }

    /* 确保 next_id 文件存在 */
    char id_path[MAX_PATH];
    get_next_id_path(id_path, sizeof(id_path));
    if (!file_exists(id_path)) {
        write_next_id(1);
    }

    LOG_INFO("数据库初始化完成: %s", g_db_base);
    return 0;
}

void db_close(void)
{
    LOG_INFO("数据库已关闭");
}

/* --- 用户表操作 --- */

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
    char path[MAX_PATH];
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

    char path[MAX_PATH];
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

    char users_dir[MAX_PATH];
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

        char filepath[MAX_PATH];
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
    char path[MAX_PATH];
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

    char users_dir[MAX_PATH];
    get_users_dir(users_dir, sizeof(users_dir));

    /* 使用跨平台 DirIterator 遍历目录 */
    DirIterator it;
    if (dir_open(&it, users_dir) != 0) return 0;

    char name_buffer[256];

    while (*count < max_results && dir_next(&it, name_buffer, sizeof(name_buffer)) == 0) {
        /* 只处理 .json 文件 */
        size_t len = strlen(name_buffer);
        if (len < 6 || strcmp(name_buffer + len - 5, ".json") != 0) continue;

        char filepath[MAX_PATH];
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

/* --- 消息表操作 (Phase 4 实现) --- */

int db_save_message(int64_t from_id, int64_t to_id, const char* content_encrypted,
                    int64_t* message_id)
{
    (void)from_id; (void)to_id; (void)content_encrypted; (void)message_id;
    LOG_DEBUG("db_save_message — Phase 4 实现");
    return 0;
}

int db_get_messages(int64_t user1_id, int64_t user2_id, int64_t offset, int64_t limit,
                    int64_t* ids, int64_t* from_ids, char** contents, int64_t* timestamps,
                    size_t* count)
{
    (void)user1_id; (void)user2_id; (void)offset; (void)limit;
    (void)ids; (void)from_ids; (void)contents; (void)timestamps; (void)count;
    LOG_DEBUG("db_get_messages — Phase 4 实现");
    return 0;
}

int db_mark_message_read(int64_t message_id)
{
    (void)message_id;
    LOG_DEBUG("db_mark_message_read — Phase 4 实现");
    return 0;
}

/* --- 好友表操作 (Phase 4 实现) --- */

int db_add_friend(int64_t user_id, int64_t friend_id)
{
    (void)user_id; (void)friend_id;
    LOG_DEBUG("db_add_friend — Phase 4 实现");
    return 0;
}

int db_remove_friend(int64_t user_id, int64_t friend_id)
{
    (void)user_id; (void)friend_id;
    LOG_DEBUG("db_remove_friend — Phase 4 实现");
    return 0;
}

int db_get_friends(int64_t user_id, int64_t* friend_ids, size_t max_count, size_t* count)
{
    (void)user_id; (void)friend_ids; (void)max_count; (void)count;
    LOG_DEBUG("db_get_friends — Phase 4 实现");
    return 0;
}

int db_check_friendship(int64_t user_id, int64_t friend_id, bool* are_friends)
{
    (void)user_id; (void)friend_id; (void)are_friends;
    LOG_DEBUG("db_check_friendship — Phase 4 实现");
    return 0;
}

/* --- 模板表操作 (Phase 5 实现) --- */

int db_create_template(const char* name, int64_t author_id, const char* css_path,
                       const char* preview_url, int64_t* template_id)
{
    (void)name; (void)author_id; (void)css_path; (void)preview_url; (void)template_id;
    LOG_DEBUG("db_create_template — Phase 5 实现");
    return 0;
}

int db_get_templates(int64_t offset, int64_t limit, int64_t* ids, char** names,
                     int64_t* author_ids, char** preview_urls, int64_t* downloads, size_t* count)
{
    (void)offset; (void)limit; (void)ids; (void)names;
    (void)author_ids; (void)preview_urls; (void)downloads; (void)count;
    LOG_DEBUG("db_get_templates — Phase 5 实现");
    return 0;
}

int db_apply_template(int64_t user_id, int64_t template_id)
{
    (void)user_id; (void)template_id;
    LOG_DEBUG("db_apply_template — Phase 5 实现");
    return 0;
}

int db_get_user_template(int64_t user_id, int64_t* template_id)
{
    (void)user_id; (void)template_id;
    LOG_DEBUG("db_get_user_template — Phase 5 实现");
    return 0;
}

int db_increment_template_downloads(int64_t template_id)
{
    (void)template_id;
    LOG_DEBUG("db_increment_template_downloads — Phase 5 实现");
    return 0;
}
