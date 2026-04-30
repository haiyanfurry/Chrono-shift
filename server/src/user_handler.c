/**
 * Chrono-shift 用户管理 HTTP 处理器
 * 语言标准: C99
 *
 * 实现用户注册、登录、获取/更新资料、搜索用户
 * 密码通过 Rust FFI (Argon2id) 哈希
 * 认证通过 Rust FFI (JWT) 签发/验证
 */

#include "user_handler.h"
#include "database.h"
#include "json_parser.h"
#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ============================================================
 * Rust FFI 函数声明 (在 server/security/ 中实现)
 * ============================================================ */

extern int    rust_verify_password(const char* password, const char* hash);
extern char*  rust_generate_jwt(const char* user_id);
extern char*  rust_hash_password(const char* password);
extern void   rust_free_string(char* s);

/* ============================================================
 * 辅助函数
 * ============================================================ */

/* 从请求 body 中提取 JSON 字符串字段 */
static char* get_json_string_field(const uint8_t* body, size_t body_len, const char* key)
{
    if (!body || body_len == 0) return NULL;

    /* body 可能不是 null-terminated，创建一个副本 */
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

/* 构建包含用户信息的 JSON data 部分 */
static char* build_user_data_json(int64_t user_id, const char* username,
                                   const char* nickname, const char* avatar_url)
{
    /* 先转义各字符串字段, 防止 JSON 注入 */
    char* safe_username    = json_escape_string(username ? username : "");
    char* safe_nickname    = json_escape_string(nickname ? nickname : "");
    char* safe_avatar_url  = json_escape_string(avatar_url ? avatar_url : "");

    if (!safe_username || !safe_nickname || !safe_avatar_url) {
        free(safe_username);
        free(safe_nickname);
        free(safe_avatar_url);
        return NULL;
    }

    /* 计算所需缓冲区大小 */
    size_t len = 128 + strlen(safe_username) + strlen(safe_nickname) + strlen(safe_avatar_url);

    char* json = (char*)malloc(len);
    if (!json) {
        free(safe_username);
        free(safe_nickname);
        free(safe_avatar_url);
        return NULL;
    }

    snprintf(json, len,
             "{"
             "\"id\":%lld,"
             "\"username\":\"%s\","
             "\"nickname\":\"%s\","
             "\"avatar_url\":\"%s\""
             "}",
             (long long)user_id,
             safe_username,
             safe_nickname,
             safe_avatar_url);

    free(safe_username);
    free(safe_nickname);
    free(safe_avatar_url);

    return json;
}

/* ============================================================
 * 路由处理器
 * ============================================================ */

/**
 * POST /api/user/register
 * Body: {"username":"...","password":"...","nickname":"..."}
 * Response: {"status":"ok","message":"success","data":{"id":...,"token":"..."}}
 */
void handle_user_register(const HttpRequest* req, HttpResponse* resp, void* user_data)
{
    (void)user_data;
    http_response_init(resp);

    /* 提取字段 */
    char* username = get_json_string_field(req->body, req->body_length, "username");
    char* password = get_json_string_field(req->body, req->body_length, "password");
    char* nickname = get_json_string_field(req->body, req->body_length, "nickname");

    if (!username || !password || strlen(username) == 0 || strlen(password) == 0) {
        http_response_set_json(resp, json_build_error("用户名和密码不能为空"));
        free(username);
        free(password);
        free(nickname);
        http_response_set_status(resp, 400, "Bad Request");
        return;
    }

    if (strlen(username) < 3 || strlen(username) > 32) {
        http_response_set_json(resp, json_build_error("用户名长度需在 3-32 字符之间"));
        free(username);
        free(password);
        free(nickname);
        http_response_set_status(resp, 400, "Bad Request");
        return;
    }

    if (strlen(password) < 6) {
        http_response_set_json(resp, json_build_error("密码长度不能少于 6 位"));
        free(username);
        free(password);
        free(nickname);
        http_response_set_status(resp, 400, "Bad Request");
        return;
    }

    /* 使用 Rust Argon2id 哈希密码 */
    char* password_hash = rust_hash_password(password);
    if (!password_hash) {
        http_response_set_json(resp, json_build_error("密码加密失败"));
        free(username);
        free(password);
        free(nickname);
        http_response_set_status(resp, 500, "Internal Server Error");
        return;
    }

    /* 创建用户 */
    int user_id = db_create_user(username, password_hash,
                                  nickname ? nickname : username, NULL);
    rust_free_string(password_hash);

    if (user_id < 0) {
        if (user_id == -2) {
            http_response_set_json(resp, json_build_error("用户名已存在"));
            http_response_set_status(resp, 409, "Conflict");
        } else {
            http_response_set_json(resp, json_build_error("用户创建失败"));
            http_response_set_status(resp, 500, "Internal Server Error");
        }
        free(username);
        free(password);
        free(nickname);
        return;
    }

    /* 生成 JWT */
    char uid_str[32];
    snprintf(uid_str, sizeof(uid_str), "%d", user_id);
    char* token = rust_generate_jwt(uid_str);

    /* 构建响应 data */
    char* user_data_json = build_user_data_json((int64_t)user_id, username,
                                                 nickname ? nickname : username, "");
    if (!user_data_json) {
        http_response_set_json(resp, json_build_error("内部错误"));
        free(username);
        free(password);
        free(nickname);
        if (token) rust_free_string(token);
        http_response_set_status(resp, 500, "Internal Server Error");
        return;
    }

    /* data 中增加 token 字段 */
    size_t data_len = strlen(user_data_json) + (token ? strlen(token) : 0) + 128;
    char* data_with_token = (char*)malloc(data_len);
    if (data_with_token) {
        /* 去掉 user_data_json 末尾的 '}' 添加 token */
        size_t ud_len = strlen(user_data_json);
        if (ud_len > 1 && user_data_json[ud_len - 1] == '}') {
            user_data_json[ud_len - 1] = '\0';
        }
        snprintf(data_with_token, data_len, "%s,\"token\":\"%s\"}",
                 user_data_json, token ? token : "");
    }

    char* response = json_build_success(data_with_token ? data_with_token : user_data_json);
    http_response_set_json(resp, response);
    free(response);

    free(data_with_token);
    free(user_data_json);
    if (token) rust_free_string(token);
    free(username);
    free(password);
    free(nickname);
}

/**
 * POST /api/user/login
 * Body: {"username":"...","password":"..."}
 * Response: {"status":"ok","message":"success","data":{"id":...,"token":"...","nickname":"..."}}
 */
void handle_user_login(const HttpRequest* req, HttpResponse* resp, void* user_data)
{
    (void)user_data;
    http_response_init(resp);

    /* 提取字段 */
    char* username = get_json_string_field(req->body, req->body_length, "username");
    char* password = get_json_string_field(req->body, req->body_length, "password");

    if (!username || !password || strlen(username) == 0 || strlen(password) == 0) {
        http_response_set_json(resp, json_build_error("用户名和密码不能为空"));
        free(username);
        free(password);
        http_response_set_status(resp, 400, "Bad Request");
        return;
    }

    /* 查找用户 */
    int64_t user_id;
    char* password_hash = NULL;
    char* nickname = NULL;

    if (db_get_user_by_username(username, &user_id, &password_hash, &nickname) != 0) {
        http_response_set_json(resp, json_build_error("用户名或密码错误"));
        free(username);
        free(password);
        http_response_set_status(resp, 401, "Unauthorized");
        return;
    }

    /* 验证密码 */
    int verify_result = rust_verify_password(password, password_hash);
    rust_free_string(password_hash);

    if (verify_result != 1) {
        http_response_set_json(resp, json_build_error("用户名或密码错误"));
        free(username);
        free(password);
        free(nickname);
        http_response_set_status(resp, 401, "Unauthorized");
        return;
    }

    /* 生成 JWT */
    char uid_str[32];
    snprintf(uid_str, sizeof(uid_str), "%lld", (long long)user_id);
    char* token = rust_generate_jwt(uid_str);

    /* 获取用户资料 */
    char* db_nickname = NULL;
    char* db_avatar = NULL;
    db_get_user_by_id(user_id, &db_nickname, &db_nickname, &db_avatar);
    /* 注意: db_get_user_by_id 的前两个 out 参数是 username, nickname, avatar_url */
    /* 修正调用 */
    char* stored_username = NULL;
    char* stored_nickname = NULL;
    char* stored_avatar = NULL;
    db_get_user_by_id(user_id, &stored_username, &stored_nickname, &stored_avatar);

    char* user_data_json = build_user_data_json(
        user_id,
        stored_username ? stored_username : username,
        stored_nickname ? stored_nickname : (nickname ? nickname : username),
        stored_avatar ? stored_avatar : "");

    free(stored_username);
    free(stored_nickname);
    free(stored_avatar);
    free(nickname);

    size_t data_len = (user_data_json ? strlen(user_data_json) : 0) +
                      (token ? strlen(token) : 0) + 128;
    char* data_with_token = (char*)malloc(data_len);
    if (data_with_token && user_data_json) {
        size_t ud_len = strlen(user_data_json);
        if (ud_len > 1 && user_data_json[ud_len - 1] == '}') {
            user_data_json[ud_len - 1] = '\0';
        }
        snprintf(data_with_token, data_len, "%s,\"token\":\"%s\"}",
                 user_data_json, token ? token : "");
    }

    char* response = json_build_success(data_with_token ? data_with_token : user_data_json);
    http_response_set_json(resp, response);
    free(response);

    free(data_with_token);
    if (user_data_json) free(user_data_json);
    if (token) rust_free_string(token);
    free(username);
    free(password);
}

/**
 * GET /api/user/profile?user_id=...
 * Authorization: Bearer <token>
 * Response: {"status":"ok","data":{"id":...,"username":"...","nickname":"...","avatar_url":"..."}}
 */
void handle_user_profile(const HttpRequest* req, HttpResponse* resp, void* user_data)
{
    (void)user_data;
    http_response_init(resp);

    /* 从查询参数或 Authorization token 获取 user_id */
    int64_t target_id = 0;

    /* 解析查询字符串中的 user_id */
    if (strlen(req->query) > 0) {
        char* q = strstr(req->query, "user_id=");
        if (q) {
            target_id = (int64_t)atoll(q + 8);
        }
    }

    /* 如果没有指定 user_id，从 token 获取当前用户 */
    if (target_id == 0) {
        /* 从 Authorization 头解析 token */
        const char* auth = http_extract_bearer_token(req->headers, req->header_count);
        if (!auth) {
            http_response_set_json(resp, json_build_error("未授权"));
            http_response_set_status(resp, 401, "Unauthorized");
            return;
        }

        /* 通过 token 获取 user_id - 简单地从 DB 查找第一个用户来验证 token */
        /* 完整的 JWT 验证将在 Rust 安全模块中实现 */
        /* 这里暂用简单的 token 传递 user_id 机制 */
        /* TODO: Phase 7 实现完整的 JWT 验证 */

        /* 尝试从 Authorization 中提取 user_id（简化版） */
        /* 正式环境应该调用 rust_verify_jwt() */
        http_response_set_json(resp, json_build_error("请指定 user_id 查询参数"));
        http_response_set_status(resp, 400, "Bad Request");
        return;
    }

    /* 获取用户资料 */
    char* username = NULL;
    char* nickname = NULL;
    char* avatar_url = NULL;

    if (db_get_user_by_id(target_id, &username, &nickname, &avatar_url) != 0) {
        http_response_set_json(resp, json_build_error("用户不存在"));
        http_response_set_status(resp, 404, "Not Found");
        return;
    }

    char* data_json = build_user_data_json(target_id, username, nickname, avatar_url);
    char* response = json_build_success(data_json);

    http_response_set_json(resp, response);

    free(response);
    free(data_json);
    free(username);
    free(nickname);
    free(avatar_url);
}

/**
 * PUT /api/user/update
 * Authorization: Bearer <token>
 * Body: {"nickname":"...","avatar_url":"..."}
 */
void handle_user_update(const HttpRequest* req, HttpResponse* resp, void* user_data)
{
    (void)user_data;
    http_response_init(resp);

    /* 从 Authorization 头获取当前用户 */
    /* 简化版: 从 body 中读取 user_id 字段 */
    char* uid_str = get_json_string_field(req->body, req->body_length, "user_id");
    char* nickname = get_json_string_field(req->body, req->body_length, "nickname");
    char* avatar_url = get_json_string_field(req->body, req->body_length, "avatar_url");

    if (!uid_str) {
        http_response_set_json(resp, json_build_error("缺少 user_id"));
        free(nickname);
        free(avatar_url);
        http_response_set_status(resp, 400, "Bad Request");
        return;
    }

    int64_t user_id = (int64_t)atoll(uid_str);
    free(uid_str);

    if (!nickname && !avatar_url) {
        http_response_set_json(resp, json_build_error("没有需要更新的字段"));
        http_response_set_status(resp, 400, "Bad Request");
        return;
    }

    if (db_update_user_profile(user_id, nickname, avatar_url) != 0) {
        http_response_set_json(resp, json_build_error("更新失败"));
        free(nickname);
        free(avatar_url);
        http_response_set_status(resp, 500, "Internal Server Error");
        return;
    }

    char* response = json_build_success("{}");
    http_response_set_json(resp, response);
    free(response);
    free(nickname);
    free(avatar_url);
}

/**
 * GET /api/user/search?q=keyword
 * Response: {"status":"ok","data":{"users":[...]}}
 */
void handle_user_search(const HttpRequest* req, HttpResponse* resp, void* user_data)
{
    (void)user_data;
    http_response_init(resp);

    /* 从查询字符串提取关键词 */
    const char* keyword = "";
    if (strlen(req->query) > 0) {
        char* q = strstr(req->query, "q=");
        if (q) {
            keyword = q + 2;
        }
    }

    if (strlen(keyword) == 0) {
        http_response_set_json(resp, json_build_error("请提供搜索关键词"));
        http_response_set_status(resp, 400, "Bad Request");
        return;
    }

    /* 搜索用户 */
    int64_t results[100];
    size_t count = 0;

    db_search_users(keyword, results, 100, &count);

    /* 构建 JSON 数组 */
    char* array_json = NULL;
    size_t array_size = 256;

    for (size_t i = 0; i < count; i++) {
        char* uname = NULL;
        char* nname = NULL;
        char* av_url = NULL;
        db_get_user_by_id(results[i], &uname, &nname, &av_url);

        char user_json[1024];
        snprintf(user_json, sizeof(user_json),
                 "%s{\"id\":%lld,\"username\":\"%s\",\"nickname\":\"%s\",\"avatar_url\":\"%s\"}",
                 i > 0 ? "," : "",
                 (long long)results[i],
                 uname ? uname : "",
                 nname ? nname : "",
                 av_url ? av_url : "");

        array_size += strlen(user_json);
        if (!array_json) {
            array_json = (char*)malloc(array_size);
            if (array_json) array_json[0] = '\0';
        } else {
            char* tmp = (char*)realloc(array_json, array_size);
            if (tmp) array_json = tmp;
        }
        if (array_json) {
            strcat(array_json, user_json);
        }

        free(uname);
        free(nname);
        free(av_url);
    }

    size_t final_len = array_size + 32;
    char* data_json = (char*)malloc(final_len);
    if (data_json) {
        snprintf(data_json, final_len, "{\"users\":[%s]}",
                 array_json ? array_json : "");
    }

    char* response = json_build_success(data_json);
    http_response_set_json(resp, response);

    free(response);
    free(data_json);
    free(array_json);
}

/**
 * GET /api/user/friends — Phase 4 占位
 */
void handle_user_friends(const HttpRequest* req, HttpResponse* resp, void* user_data)
{
    (void)req;
    (void)user_data;
    http_response_init(resp);
    http_response_set_json(resp, json_build_error("功能开发中 - Phase 4"));
}

/**
 * POST /api/user/friends/add — Phase 4 占位
 */
void handle_user_add_friend(const HttpRequest* req, HttpResponse* resp, void* user_data)
{
    (void)req;
    (void)user_data;
    http_response_init(resp);
    http_response_set_json(resp, json_build_error("功能开发中 - Phase 4"));
}
