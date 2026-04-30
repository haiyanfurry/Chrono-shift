/**
 * Chrono-shift 消息处理 HTTP/WS 处理器
 * 语言标准: C99
 *
 * 实现消息发送、历史获取、WebSocket 实时转发
 * 认证通过 JWT Bearer Token (Authorization header)
 * WebSocket 会话管理：连接后客户端发送 {"type":"auth","user_id":N}
 */

#include "message_handler.h"
#include "database.h"
#include "websocket.h"
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
 * WebSocket 会话管理
 * ============================================================ */

#define MAX_WS_SESSIONS 256

typedef struct {
    WsConnection* conn;
    int64_t       user_id;
    char          username[64];
    int           authenticated;
} WsSession;

static WsSession g_ws_sessions[MAX_WS_SESSIONS];
static int       g_ws_session_count = 0;

/* 查找指定用户 ID 的 WebSocket 连接 */
static WsConnection* find_user_ws(int64_t user_id)
{
    for (int i = 0; i < g_ws_session_count; i++) {
        if (g_ws_sessions[i].authenticated &&
            g_ws_sessions[i].user_id == user_id) {
            return g_ws_sessions[i].conn;
        }
    }
    return NULL;
}

/* 查找指定连接的会话索引 */
static int find_session_by_conn(WsConnection* conn)
{
    for (int i = 0; i < g_ws_session_count; i++) {
        if (g_ws_sessions[i].conn == conn) return i;
    }
    return -1;
}

/* 添加新会话 */
static int add_ws_session(WsConnection* conn)
{
    if (g_ws_session_count >= MAX_WS_SESSIONS) return -1;
    g_ws_sessions[g_ws_session_count].conn = conn;
    g_ws_sessions[g_ws_session_count].user_id = 0;
    g_ws_sessions[g_ws_session_count].username[0] = '\0';
    g_ws_sessions[g_ws_session_count].authenticated = 0;
    return g_ws_session_count++;
}

/* 移除会话 */
static void remove_ws_session(int idx)
{
    if (idx < 0 || idx >= g_ws_session_count) return;
    if (idx < g_ws_session_count - 1) {
        memmove(&g_ws_sessions[idx], &g_ws_sessions[idx + 1],
                (size_t)(g_ws_session_count - idx - 1) * sizeof(WsSession));
    }
    g_ws_session_count--;
}

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

/* 从 JWT token 中提取 user_id (通过 Rust FFI) */
static int64_t get_user_id_from_token(const char* token)
{
    if (!token) return 0;

    char* uid_str = rust_verify_jwt(token);
    if (!uid_str) return 0;

    int64_t user_id = (int64_t)atoll(uid_str);
    rust_free_string(uid_str);
    return user_id;
}

/* 从 Authorization header 提取并验证 JWT, 返回 user_id */
static int64_t authenticate_request(const HttpRequest* req)
{
    const char* token = http_extract_bearer_token(req->headers, req->header_count);
    if (!token) return 0;

    return get_user_id_from_token(token);
}

/* 构建单条消息的 JSON 对象 (不包含外层包装) */
static char* build_message_json(int64_t id, int64_t from_id, int64_t to_id,
                                 const char* content, int64_t timestamp, int is_read)
{
    char* safe_content = json_escape_string(content ? content : "");
    if (!safe_content) return NULL;

    size_t len = strlen(safe_content) + 256;
    char* json = (char*)malloc(len);
    if (!json) {
        free(safe_content);
        return NULL;
    }

    snprintf(json, len,
             "{"
             "\"id\":%lld,"
             "\"from_id\":%lld,"
             "\"to_id\":%lld,"
             "\"content\":\"%s\","
             "\"timestamp\":%lld,"
             "\"is_read\":%s"
             "}",
             (long long)id, (long long)from_id, (long long)to_id,
             safe_content, (long long)timestamp,
             is_read ? "true" : "false");

    free(safe_content);
    return json;
}

/* ============================================================
 * HTTP API 实现
 * ============================================================ */

/**
 * POST /api/message/send
 * Body: { "to_user_id": number, "content": string }
 * Auth: Bearer JWT (from_user_id)
 * 返回消息发送结果，如果接收方在线则通过 WebSocket 实时推送
 */
void handle_send_message(const HttpRequest* req, HttpResponse* resp, void* user_data)
{
    (void)user_data;
    http_response_init(resp);

    /* 认证 */
    int64_t from_user_id = authenticate_request(req);
    if (from_user_id <= 0) {
        http_response_set_json(resp, json_build_error("未授权，请先登录"));
        http_response_set_status(resp, 401, "Unauthorized");
        return;
    }

    /* 解析请求参数 */
    int64_t to_user_id = get_json_number_field(req->body, req->body_length, "to_user_id");
    char* content = get_json_string_field(req->body, req->body_length, "content");

    if (!content || strlen(content) == 0) {
        http_response_set_json(resp, json_build_error("消息内容不能为空"));
        free(content);
        http_response_set_status(resp, 400, "Bad Request");
        return;
    }

    if (to_user_id <= 0) {
        http_response_set_json(resp, json_build_error("接收方用户 ID 无效"));
        free(content);
        http_response_set_status(resp, 400, "Bad Request");
        return;
    }

    if (to_user_id == from_user_id) {
        http_response_set_json(resp, json_build_error("不能给自己发送消息"));
        free(content);
        http_response_set_status(resp, 400, "Bad Request");
        return;
    }

    /* 保存消息到数据库 */
    int64_t message_id = 0;
    int ret = db_save_message(from_user_id, to_user_id, content, &message_id);

    if (ret != 0) {
        http_response_set_json(resp, json_build_error("消息保存失败"));
        free(content);
        http_response_set_status(resp, 500, "Internal Server Error");
        return;
    }

    int64_t now = (int64_t)time(NULL);

    /* 构建响应数据 */
    char data_buf[256];
    snprintf(data_buf, sizeof(data_buf),
             "{\"message_id\":%lld,\"timestamp\":%lld}",
             (long long)message_id, (long long)now);

    char* response = json_build_success(data_buf);
    http_response_set_json(resp, response);
    free(response);

    /* WebSocket 实时推送：如果接收方在线，转发消息 */
    WsConnection* target_conn = find_user_ws(to_user_id);
    if (target_conn) {
        char* ws_json = build_message_json(message_id, from_user_id, to_user_id,
                                            content, now, 0);
        if (ws_json) {
            ws_send_text(target_conn, ws_json);
            free(ws_json);
        }
    }

    free(content);
}

/**
 * GET /api/message/list?user_id=X&offset=0&limit=50
 * Auth: Bearer JWT
 * 返回两个用户之间的消息历史，按时间倒序
 */
void handle_get_messages(const HttpRequest* req, HttpResponse* resp, void* user_data)
{
    (void)user_data;
    http_response_init(resp);

    /* 认证 */
    int64_t current_user_id = authenticate_request(req);
    if (current_user_id <= 0) {
        http_response_set_json(resp, json_build_error("未授权，请先登录"));
        http_response_set_status(resp, 401, "Unauthorized");
        return;
    }

    /* 解析查询参数 */
    int64_t partner_id = get_query_int(req->query, "user_id");
    int64_t offset = get_query_int(req->query, "offset");
    int64_t limit  = get_query_int(req->query, "limit");

    if (partner_id <= 0) {
        http_response_set_json(resp, json_build_error("请指定 user_id 查询参数"));
        http_response_set_status(resp, 400, "Bad Request");
        return;
    }

    if (offset < 0) offset = 0;
    if (limit <= 0 || limit > 100) limit = 50;

    /* 分配缓冲区 */
    int64_t* ids        = (int64_t*)malloc((size_t)limit * sizeof(int64_t));
    int64_t* from_ids   = (int64_t*)malloc((size_t)limit * sizeof(int64_t));
    char**   contents   = (char**)calloc((size_t)limit, sizeof(char*));
    int64_t* timestamps = (int64_t*)malloc((size_t)limit * sizeof(int64_t));

    if (!ids || !from_ids || !contents || !timestamps) {
        free(ids); free(from_ids); free(contents); free(timestamps);
        http_response_set_json(resp, json_build_error("内存不足"));
        http_response_set_status(resp, 500, "Internal Server Error");
        return;
    }

    size_t count = 0;
    int ret = db_get_messages(current_user_id, partner_id, offset, limit,
                               ids, from_ids, contents, timestamps, &count);

    if (ret != 0) {
        free(ids); free(from_ids); free(contents); free(timestamps);
        http_response_set_json(resp, json_build_error("获取消息失败"));
        http_response_set_status(resp, 500, "Internal Server Error");
        return;
    }

    /* 构建消息数组 JSON */
    char* messages_json = NULL;
    size_t total_len = 16; /* 预留 "[" 和 "]" 和可能的 null */

    /* 先估算总长度 */
    for (size_t i = 0; i < count; i++) {
        char* safe = json_escape_string(contents[i] ? contents[i] : "");
        total_len += (safe ? strlen(safe) : 0) + 128;
        free(safe);
    }

    messages_json = (char*)malloc(total_len);
    if (!messages_json) {
        for (size_t i = 0; i < count; i++) free(contents[i]);
        free(ids); free(from_ids); free(contents); free(timestamps);
        http_response_set_json(resp, json_build_error("内存不足"));
        http_response_set_status(resp, 500, "Internal Server Error");
        return;
    }

    size_t pos = 0;
    pos += snprintf(messages_json + pos, total_len - pos, "[");

    for (size_t i = 0; i < count; i++) {
        /* 标记已读 (只针对发给当前用户的消息) */
        int is_read = (from_ids[i] != current_user_id) ? 1 : 0;

        if (pos >= total_len) break;
        if (i > 0) {
            pos += snprintf(messages_json + pos, total_len - pos, ",");
        }

        char* msg_json = build_message_json(
            ids[i], from_ids[i], 
            /* 确定 to_id: 如果 from_id 是当前用户，则 partner 是接收方 */
            (from_ids[i] == current_user_id) ? partner_id : current_user_id,
            contents[i] ? contents[i] : "", timestamps[i], is_read);

        if (msg_json) {
            size_t remaining = (total_len > pos) ? total_len - pos : 0;
            size_t written = strlen(msg_json);
            if (written < remaining - 1) {
                memcpy(messages_json + pos, msg_json, written);
                pos += written;
            }
            free(msg_json);
        }

        free(contents[i]);
    }

    if (pos < total_len) {
        messages_json[pos] = '\0';
    }
    messages_json[total_len - 1] = '\0';

    /* 构建最终响应: {"messages": [...]} */
    size_t data_len = strlen(messages_json) + 32;
    char* data_json = (char*)malloc(data_len);
    if (data_json) {
        snprintf(data_json, data_len, "{\"messages\":%s}", messages_json);
    }

    char* response = json_build_success(data_json ? data_json : "{\"messages\":[]}");
    http_response_set_json(resp, response);

    free(response);
    free(data_json);
    free(messages_json);
    free(ids);
    free(from_ids);
    free(contents);
    free(timestamps);
}

/* ============================================================
 * WebSocket 处理器
 * ============================================================ */

/**
 * WebSocket 消息到达处理
 * 客户端发送 JSON 消息:
 *   - 认证: {"type":"auth","user_id":N,"token":"..."}
 *   - 聊天: {"type":"message","to_user_id":N,"content":"..."}
 *   - 心跳: {"type":"ping"}
 */
void ws_on_message(WsConnection* conn, enum WsOpcode opcode,
                   const uint8_t* data, size_t length)
{
    if (opcode != WS_OPCODE_TEXT && opcode != WS_OPCODE_BINARY) {
        return; /* 忽略非文本帧 */
    }

    if (!data || length == 0) return;

    /* 复制数据为 null-terminated 字符串 */
    char* text = (char*)malloc(length + 1);
    if (!text) return;
    memcpy(text, data, length);
    text[length] = '\0';

    /* 解析 JSON */
    JsonValue* root = json_parse(text);
    free(text);

    if (!root) return;

    JsonValue* type_val = json_object_get(root, "type");
    if (!type_val || type_val->type != JSON_STRING) {
        json_value_free(root);
        return;
    }

    const char* type = type_val->string_val;

    if (strcmp(type, "auth") == 0) {
        /* ====== 客户端认证 ====== */
        JsonValue* v_uid = json_object_get(root, "user_id");
        JsonValue* v_token = json_object_get(root, "token");

        if (v_uid && v_uid->type == JSON_NUMBER && v_token && v_token->type == JSON_STRING) {
            int64_t user_id = (int64_t)v_uid->number_val;

            /* 验证 JWT token */
            char* verified_uid = rust_verify_jwt(v_token->string_val);
            if (verified_uid) {
                int64_t token_uid = (int64_t)atoll(verified_uid);
                rust_free_string(verified_uid);

                if (token_uid == user_id) {
                    /* 查找并更新会话 */
                    int idx = find_session_by_conn(conn);
                    if (idx >= 0) {
                        g_ws_sessions[idx].user_id = user_id;
                        g_ws_sessions[idx].authenticated = 1;

                        /* 获取用户名 */
                        char* username = NULL;
                        char* nickname = NULL;
                        char* avatar = NULL;
                        if (db_get_user_by_id(user_id, &username, &nickname, &avatar) == 0) {
                            strncpy(g_ws_sessions[idx].username,
                                    nickname ? nickname : (username ? username : ""),
                                    sizeof(g_ws_sessions[idx].username) - 1);
                            free(username);
                            free(nickname);
                            free(avatar);
                        }

                        char auth_ok[128];
                        snprintf(auth_ok, sizeof(auth_ok),
                                 "{\"type\":\"auth_ok\",\"user_id\":%lld}",
                                 (long long)user_id);
                        ws_send_text(conn, auth_ok);
                        LOG_INFO("WebSocket 用户已认证: user_id=%lld", (long long)user_id);
                    }
                }
            }
        }
    } else if (strcmp(type, "message") == 0) {
        /* ====== 聊天消息 ====== */
        /* 查找当前连接对应的用户 */
        int idx = find_session_by_conn(conn);
        if (idx < 0 || !g_ws_sessions[idx].authenticated) {
            ws_send_text(conn, "{\"type\":\"error\",\"message\":\"未认证\"}");
            json_value_free(root);
            return;
        }

        int64_t from_id = g_ws_sessions[idx].user_id;
        JsonValue* v_to   = json_object_get(root, "to_user_id");
        JsonValue* v_cont = json_object_get(root, "content");

        if (!v_to || v_to->type != JSON_NUMBER || !v_cont || v_cont->type != JSON_STRING) {
            ws_send_text(conn, "{\"type\":\"error\",\"message\":\"参数无效\"}");
            json_value_free(root);
            return;
        }

        int64_t to_id = (int64_t)v_to->number_val;
        const char* content = v_cont->string_val;

        /* 保存到数据库 */
        int64_t message_id = 0;
        int ret = db_save_message(from_id, to_id, content, &message_id);
        int64_t now = (int64_t)time(NULL);

        /* 发送确认回发送者 */
        char confirm[128];
        snprintf(confirm, sizeof(confirm),
                 "{\"type\":\"sent\",\"message_id\":%lld,\"timestamp\":%lld}",
                 (long long)message_id, (long long)now);
        ws_send_text(conn, confirm);

        /* 如果接收方在线，转发消息 */
        if (ret == 0) {
            char* msg_json = build_message_json(message_id, from_id, to_id,
                                                 content, now, 0);
            if (msg_json) {
                /* 添加 type 字段用于 WS 路由 */
                size_t ws_len = strlen(msg_json) + 32;
                char* ws_msg = (char*)malloc(ws_len);
                if (ws_msg) {
                    /* 去掉尾部 }，添加 type */
                    size_t mj_len = strlen(msg_json);
                    if (mj_len > 1 && msg_json[mj_len - 1] == '}') {
                        msg_json[mj_len - 1] = '\0';
                    }
                    snprintf(ws_msg, ws_len, "%s,\"type\":\"new_message\"}",
                             msg_json);
                    free(msg_json);

                    WsConnection* target = find_user_ws(to_id);
                    if (target) {
                        ws_send_text(target, ws_msg);
                    }
                    free(ws_msg);
                }
            }
        }

    } else if (strcmp(type, "ping") == 0) {
        /* ====== 心跳 ====== */
        ws_send_text(conn, "{\"type\":\"pong\"}");

    } else if (strcmp(type, "read") == 0) {
        /* ====== 已读回执 ====== */
        JsonValue* v_msg_id = json_object_get(root, "message_id");
        if (v_msg_id && v_msg_id->type == JSON_NUMBER) {
            int64_t msg_id = (int64_t)v_msg_id->number_val;
            db_mark_message_read(msg_id);
        }
    }

    json_value_free(root);
}

/**
 * WebSocket 连接关闭处理
 */
void ws_on_close(WsConnection* conn, uint16_t code, const char* reason)
{
    int idx = find_session_by_conn(conn);
    if (idx >= 0) {
        LOG_DEBUG("WebSocket 连接关闭: user_id=%lld, code=%u, reason=%s",
                  (long long)g_ws_sessions[idx].user_id, (unsigned)code,
                  reason ? reason : "");
        g_ws_sessions[idx].authenticated = 0;
        remove_ws_session(idx);
    }
}

/**
 * WebSocket 新连接处理
 */
void ws_on_connect(WsConnection* conn)
{
    int idx = add_ws_session(conn);
    if (idx < 0) {
        LOG_ERROR("WebSocket 会话已达上限，拒绝新连接");
        ws_close(conn, 1008, "会话数上限");
        return;
    }

    char peer_addr[64] = "";
    ws_get_peer_addr(conn, peer_addr, sizeof(peer_addr));
    LOG_INFO("WebSocket 新连接: addr=%s", peer_addr);

    /* 发送欢迎消息，提示客户端进行认证 */
    ws_send_text(conn, "{\"type\":\"welcome\",\"version\":\"1.0\"}");
}
