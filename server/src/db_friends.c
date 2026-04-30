/**
 * Chrono-shift 数据库好友模块
 * 语言标准: C99
 *
 * 包含：好友列表读写、好友关系 CRUD
 */

#include "database.h"
#include "db_core.h"
#include "server.h"
#include "json_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "platform_compat.h"

/* ============================================================
 * 好友列表 JSON 读写辅助
 * ============================================================ */

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

/* ============================================================
 * 好友 CRUD
 * ============================================================ */

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
