/**
 * Chrono-shift 数据库消息模块
 * 语言标准: C99
 *
 * 包含：消息 CRUD 操作
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
 * 消息排序辅助
 * ============================================================ */

static int sort_messages_by_time(const void* a, const void* b)
{
    const int64_t* va = (const int64_t*)a;
    const int64_t* vb = (const int64_t*)b;
    if (va[2] < vb[2]) return 1;
    if (va[2] > vb[2]) return -1;
    return 0;
}

/* ============================================================
 * 消息 CRUD
 * ============================================================ */

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
