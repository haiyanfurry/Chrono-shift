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

/* ============================================================
 * 内部常量
 * ============================================================ */

#define DB_DIR_MODE  0755
#define USER_DIR     "users"
#define MESSAGE_DIR  "messages"
#define FRIENDSHIP_DIR "friendships"
#define TEMPLATE_DIR "templates"
#define NEXT_ID_FILE "next_id.txt"
#define DB_MAX_PATH  1024

static char g_db_base[DB_MAX_PATH] = {0};

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

/* 获取消息文件路径 */
static void get_message_path(int64_t message_id, char* path, size_t path_size)
{
    snprintf(path, path_size, "%s/%s/%lld.json",
             g_db_base, MESSAGE_DIR, (long long)message_id);
}

/* 获取消息目录路径 */
static void get_messages_dir(char* path, size_t path_size)
{
    snprintf(path, path_size, "%s/%s", g_db_base, MESSAGE_DIR);
}

/* 获取好友列表文件路径 */
static void get_friendship_path(int64_t user_id, char* path, size_t path_size)
{
    snprintf(path, path_size, "%s/%s/%lld.json",
             g_db_base, FRIENDSHIP_DIR, (long long)user_id);
}

/* 获取好友目录路径 */
static void get_friendships_dir(char* path, size_t path_size)
{
    snprintf(path, path_size, "%s/%s", g_db_base, FRIENDSHIP_DIR);
}

/* 获取模板文件路径 */
static void get_template_path(int64_t template_id, char* path, size_t path_size)
{
    snprintf(path, path_size, "%s/%s/%lld.json",
             g_db_base, TEMPLATE_DIR, (long long)template_id);
}

/* 获取模板目录路径 */
static void get_templates_dir(char* path, size_t path_size)
{
    snprintf(path, path_size, "%s/%s", g_db_base, TEMPLATE_DIR);
}

static int64_t read_next_id(void)
{
    char path[DB_MAX_PATH];
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
    char path[DB_MAX_PATH];
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

    char users_dir[DB_MAX_PATH];
    get_users_dir(users_dir, sizeof(users_dir));
    if (ensure_dir(users_dir) != 0) {
        LOG_ERROR("创建用户目录失败: %s", users_dir);
        return -1;
    }

    /* 确保 messages 目录存在 */
    char msg_dir[DB_MAX_PATH];
    get_messages_dir(msg_dir, sizeof(msg_dir));
    if (ensure_dir(msg_dir) != 0) {
        LOG_ERROR("创建消息目录失败: %s", msg_dir);
        return -1;
    }

    /* 确保 friendships 目录存在 */
    char fr_dir[DB_MAX_PATH];
    get_friendships_dir(fr_dir, sizeof(fr_dir));
    if (ensure_dir(fr_dir) != 0) {
        LOG_ERROR("创建好友目录失败: %s", fr_dir);
        return -1;
    }

    /* 确保 templates 目录存在 */
    char tmpl_dir[DB_MAX_PATH];
    get_templates_dir(tmpl_dir, sizeof(tmpl_dir));
    if (ensure_dir(tmpl_dir) != 0) {
        LOG_ERROR("创建模板目录失败: %s", tmpl_dir);
        return -1;
    }

    /* 确保 next_id 文件存在 */
    char id_path[DB_MAX_PATH];
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

/* --- 消息表操作 --- */

int db_save_message(int64_t from_id, int64_t to_id, const char* content_encrypted,
                    int64_t* message_id)
{
    if (!content_encrypted || !message_id) {
        LOG_ERROR("db_save_message: 参数无效");
        return -1;
    }

    /* 分配消息 ID */
    int64_t msg_id = allocate_id();
    if (msg_id < 0) {
        LOG_ERROR("分配消息 ID 失败");
        return -1;
    }

    /* 转义内容防止 JSON 注入 */
    char* safe_content = json_escape_string(content_encrypted);
    if (!safe_content) {
        return -1;
    }

    /* 构建 JSON */
    size_t len = 256 + strlen(safe_content);
    char* json = (char*)malloc(len);
    if (!json) {
        free(safe_content);
        return -1;
    }

    int64_t now = (int64_t)time(NULL);
    snprintf(json, len,
             "{"
             "\"id\":%lld,"
             "\"from_id\":%lld,"
             "\"to_id\":%lld,"
             "\"content\":\"%s\","
             "\"timestamp\":%lld,"
             "\"is_read\":false"
             "}",
             (long long)msg_id, (long long)from_id, (long long)to_id,
             safe_content, (long long)now);

    free(safe_content);

    /* 写入文件 */
    char path[DB_MAX_PATH];
    get_message_path(msg_id, path, sizeof(path));
    FILE* f = fopen(path, "w");
    if (!f) {
        LOG_ERROR("写入消息文件失败: %s", path);
        free(json);
        return -1;
    }
    fprintf(f, "%s\n", json);
    fclose(f);
    free(json);

    *message_id = msg_id;
    LOG_DEBUG("消息已保存: id=%lld, from=%lld, to=%lld",
              (long long)msg_id, (long long)from_id, (long long)to_id);
    return 0;
}

static int sort_messages_by_time(const void* a, const void* b)
{
    const int64_t* va = (const int64_t*)a;
    const int64_t* vb = (const int64_t*)b;
    if (va[2] < vb[2]) return 1;
    if (va[2] > vb[2]) return -1;
    return 0;
}

int db_get_messages(int64_t user1_id, int64_t user2_id, int64_t offset, int64_t limit,
                    int64_t* ids, int64_t* from_ids, char** contents, int64_t* timestamps,
                    size_t* count)
{
    if (!ids || !from_ids || !contents || !timestamps || !count) return -1;
    *count = 0;

    char msg_dir[DB_MAX_PATH];
    get_messages_dir(msg_dir, sizeof(msg_dir));

    /* 使用临时数组存储 (id, from_id, timestamp) 三元组 */
    typedef struct {
        int64_t id;
        int64_t from_id;
        int64_t timestamp;
    } MsgEntry;

    MsgEntry entries[4096];
    size_t total = 0;

    /* 遍历消息目录 */
    DirIterator it;
    if (dir_open(&it, msg_dir) != 0) return 0;

    char name_buffer[256];
    while (total < 4096 && dir_next(&it, name_buffer, sizeof(name_buffer)) == 0) {
        size_t len = strlen(name_buffer);
        if (len < 6 || strcmp(name_buffer + len - 5, ".json") != 0) continue;

        char filepath[DB_MAX_PATH];
        snprintf(filepath, sizeof(filepath), "%s/%s", msg_dir, name_buffer);

        char* content = read_file_content(filepath);
        if (!content) continue;

        JsonValue* root = json_parse(content);
        free(content);
        if (!root) continue;

        JsonValue* v_from = json_object_get(root, "from_id");
        JsonValue* v_to   = json_object_get(root, "to_id");
        JsonValue* v_id   = json_object_get(root, "id");
        JsonValue* v_ts   = json_object_get(root, "timestamp");

        if (v_from && v_to && v_id && v_ts &&
            v_from->type == JSON_NUMBER && v_to->type == JSON_NUMBER &&
            v_id->type == JSON_NUMBER && v_ts->type == JSON_NUMBER) {
            int64_t from_id_val = (int64_t)v_from->number_val;
            int64_t to_id_val   = (int64_t)v_to->number_val;

            /* 检查是否属于这两个用户之间的对话 */
            if ((from_id_val == user1_id && to_id_val == user2_id) ||
                (from_id_val == user2_id && to_id_val == user1_id)) {
                entries[total].id        = (int64_t)v_id->number_val;
                entries[total].from_id   = from_id_val;
                entries[total].timestamp = (int64_t)v_ts->number_val;
                total++;
            }
        }

        json_value_free(root);
    }
    dir_close(&it);

    /* 按时间降序排序 (最新在前) */
    qsort(entries, total, sizeof(MsgEntry), sort_messages_by_time);

    /* 应用 offset 和 limit，按升序返回 (旧到新) */
    size_t start = 0;
    if ((size_t)offset < total) {
        start = total - (size_t)offset - 1;
        if ((size_t)limit < total) {
            size_t begin = (start >= (size_t)limit - 1) ? start - (size_t)limit + 1 : 0;
            start = begin;
        } else {
            start = 0;
        }
    }

    size_t fetched = 0;
    for (size_t i = start; i < total && fetched < (size_t)limit; i++, fetched++) {
        ids[fetched]        = entries[i].id;
        from_ids[fetched]   = entries[i].from_id;
        timestamps[fetched] = entries[i].timestamp;

        /* 读取消息内容 */
        char path[DB_MAX_PATH];
        get_message_path(entries[i].id, path, sizeof(path));
        char* msg_content = read_file_content(path);
        if (msg_content) {
            contents[fetched] = json_extract_string(msg_content, "content");
            free(msg_content);
            if (!contents[fetched]) contents[fetched] = strdup("");
        } else {
            contents[fetched] = strdup("");
        }
    }
    *count = fetched;
    return 0;
}

int db_mark_message_read(int64_t message_id)
{
    char path[DB_MAX_PATH];
    get_message_path(message_id, path, sizeof(path));

    char* content = read_file_content(path);
    if (!content) {
        LOG_ERROR("标记已读: 消息不存在 %lld", (long long)message_id);
        return -1;
    }

    /* 简单替换 "is_read":false 为 "is_read":true */
    char* read_pos = strstr(content, "\"is_read\":false");
    if (read_pos) {
        memcpy(read_pos, "\"is_read\":true ", 15);
    }

    FILE* f = fopen(path, "w");
    if (!f) {
        free(content);
        return -1;
    }
    fprintf(f, "%s", content);
    fclose(f);
    free(content);

    LOG_DEBUG("消息已标记已读: %lld", (long long)message_id);
    return 0;
}

/* --- 好友表操作 --- */

/* 读取好友列表 JSON 文件 */
static int read_friends_list(int64_t user_id, int64_t** friend_ids, size_t* count)
{
    char path[DB_MAX_PATH];
    get_friendship_path(user_id, path, sizeof(path));

    char* content = read_file_content(path);
    if (!content) {
        *count = 0;
        *friend_ids = NULL;
        return 0; /* 文件不存在不算错误 */
    }

    JsonValue* root = json_parse(content);
    free(content);
    if (!root) return -1;

    JsonValue* arr = json_object_get(root, "friend_ids");
    if (!arr || arr->type != JSON_ARRAY) {
        json_value_free(root);
        return -1;
    }

    int arr_len = json_array_length(arr);
    if (arr_len <= 0) {
        *count = 0;
        *friend_ids = NULL;
        json_value_free(root);
        return 0;
    }

    *friend_ids = (int64_t*)malloc((size_t)arr_len * sizeof(int64_t));
    if (!*friend_ids) {
        json_value_free(root);
        return -1;
    }

    for (int i = 0; i < arr_len; i++) {
        JsonValue* v = json_array_get(arr, (size_t)i);
        if (v && v->type == JSON_NUMBER) {
            (*friend_ids)[i] = (int64_t)v->number_val;
        } else {
            (*friend_ids)[i] = 0;
        }
    }

    *count = (size_t)arr_len;
    json_value_free(root);
    return 0;
}

/* 写入好友列表 JSON 文件 */
static int write_friends_list(int64_t user_id, const int64_t* friend_ids, size_t count)
{
    /* 计算 JSON 大小 */
    size_t len = 64 + count * 24;
    char* json = (char*)malloc(len);
    if (!json) return -1;

    char* ptr = json;
    ptr += sprintf(ptr, "{\"user_id\":%lld,\"friend_ids\":[", (long long)user_id);

    for (size_t i = 0; i < count; i++) {
        if (i > 0) *ptr++ = ',';
        ptr += sprintf(ptr, "%lld", (long long)friend_ids[i]);
    }

    ptr += sprintf(ptr, "]}");

    char path[DB_MAX_PATH];
    get_friendship_path(user_id, path, sizeof(path));

    FILE* f = fopen(path, "w");
    if (!f) {
        free(json);
        return -1;
    }
    fprintf(f, "%s\n", json);
    fclose(f);
    free(json);
    return 0;
}

int db_add_friend(int64_t user_id, int64_t friend_id)
{
    if (user_id <= 0 || friend_id <= 0 || user_id == friend_id) {
        LOG_ERROR("db_add_friend: 参数无效");
        return -1;
    }

    int64_t* ids = NULL;
    size_t count = 0;
    int ret = read_friends_list(user_id, &ids, &count);
    if (ret != 0) return -1;

    /* 检查是否已是好友 */
    for (size_t i = 0; i < count; i++) {
        if (ids[i] == friend_id) {
            free(ids);
            LOG_WARN("已是好友: %lld -> %lld", (long long)user_id, (long long)friend_id);
            return -2;
        }
    }

    /* 添加好友 */
    int64_t* new_ids = (int64_t*)realloc(ids, (count + 1) * sizeof(int64_t));
    if (!new_ids) {
        free(ids);
        return -1;
    }
    new_ids[count] = friend_id;
    count++;

    ret = write_friends_list(user_id, new_ids, count);
    free(new_ids);
    if (ret != 0) return -1;

    /* 双向添加 (好友也加自己) */
    int64_t* fr_ids = NULL;
    size_t fr_count = 0;
    ret = read_friends_list(friend_id, &fr_ids, &fr_count);
    if (ret == 0) {
        /* 检查是否已有 */
        int already = 0;
        for (size_t i = 0; i < fr_count; i++) {
            if (fr_ids[i] == user_id) { already = 1; break; }
        }
        if (!already) {
            int64_t* new_fr_ids = (int64_t*)realloc(fr_ids, (fr_count + 1) * sizeof(int64_t));
            if (new_fr_ids) {
                new_fr_ids[fr_count] = user_id;
                write_friends_list(friend_id, new_fr_ids, fr_count + 1);
                free(new_fr_ids);
            }
        } else {
            free(fr_ids);
        }
    }

    LOG_INFO("好友添加成功: user=%lld, friend=%lld",
             (long long)user_id, (long long)friend_id);
    return 0;
}

int db_remove_friend(int64_t user_id, int64_t friend_id)
{
    if (user_id <= 0 || friend_id <= 0) return -1;

    int64_t* ids = NULL;
    size_t count = 0;
    int ret = read_friends_list(user_id, &ids, &count);
    if (ret != 0) return -1;

    size_t new_count = 0;
    for (size_t i = 0; i < count; i++) {
        if (ids[i] != friend_id) {
            ids[new_count++] = ids[i];
        }
    }

    if (new_count == count) {
        free(ids);
        LOG_WARN("好友不存在: %lld -> %lld", (long long)user_id, (long long)friend_id);
        return -2;
    }

    if (new_count > 0) {
        ret = write_friends_list(user_id, ids, new_count);
    } else {
        /* 删除文件表示无好友 */
        char path[DB_MAX_PATH];
        get_friendship_path(user_id, path, sizeof(path));
        remove(path);
        ret = 0;
    }
    free(ids);
    if (ret != 0) return ret;

    /* 双向删除 */
    int64_t* fr_ids = NULL;
    size_t fr_count = 0;
    ret = read_friends_list(friend_id, &fr_ids, &fr_count);
    if (ret == 0) {
        size_t new_fr_count = 0;
        for (size_t i = 0; i < fr_count; i++) {
            if (fr_ids[i] != user_id) {
                fr_ids[new_fr_count++] = fr_ids[i];
            }
        }
        if (new_fr_count > 0) {
            write_friends_list(friend_id, fr_ids, new_fr_count);
        } else {
            char path[DB_MAX_PATH];
            get_friendship_path(friend_id, path, sizeof(path));
            remove(path);
        }
        free(fr_ids);
    }

    LOG_INFO("好友删除成功: user=%lld, friend=%lld",
             (long long)user_id, (long long)friend_id);
    return 0;
}

int db_get_friends(int64_t user_id, int64_t* friend_ids, size_t max_count, size_t* count)
{
    if (!friend_ids || !count) return -1;
    *count = 0;

    int64_t* ids = NULL;
    size_t total = 0;
    int ret = read_friends_list(user_id, &ids, &total);
    if (ret != 0) return ret;

    size_t copy_count = (total < max_count) ? total : max_count;
    for (size_t i = 0; i < copy_count; i++) {
        friend_ids[i] = ids[i];
    }
    *count = copy_count;
    free(ids);
    return 0;
}

int db_check_friendship(int64_t user_id, int64_t friend_id, bool* are_friends)
{
    if (!are_friends) return -1;
    *are_friends = false;

    int64_t* ids = NULL;
    size_t count = 0;
    int ret = read_friends_list(user_id, &ids, &count);
    if (ret != 0) return ret;

    for (size_t i = 0; i < count; i++) {
        if (ids[i] == friend_id) {
            *are_friends = true;
            break;
        }
    }

    free(ids);
    return 0;
}

/* --- 模板表操作 --- */

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
             g_db_base, TEMPLATE_DIR, (long long)user_id);

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
             g_db_base, TEMPLATE_DIR, (long long)user_id);

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