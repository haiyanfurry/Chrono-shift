/**
 * Chrono-shift 文件存储 HTTP 处理器
 * 语言标准: C99
 *
 * 功能:
 * - 文件上传 (POST /api/file/upload)
 * - 静态文件服务 (GET /api/file/...)
 * - 头像上传 (POST /api/avatar/upload)
 * - MIME 类型自动检测
 * - 扩展名白名单校验
 * - 路径穿越防护
 * - 文件魔数校验 (头像)
 */

#include "file_handler.h"
#include "server.h"
#include "platform_compat.h"
#include "database.h"
#include "json_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ============================================================
 * 常量定义
 * ============================================================ */

#define FILE_PATH_MAX       1024
#define MAX_FILE_SIZE       (50 * 1024 * 1024)  /* 50MB */
#define UPLOADS_DIR         "uploads"
#define AVATARS_DIR         "avatars"

/* 允许上传的文件扩展名白名单 */
static const char* ALLOWED_EXTENSIONS[] = {
    "png", "jpg", "jpeg", "gif", "webp", "svg", "bmp", "ico",
    "txt", "pdf", "doc", "docx", "xls", "xlsx", "zip", "tar", "gz",
    "mp3", "mp4", "webm",
    NULL
};

/* 文件扩展名 → MIME 类型映射 */
typedef struct {
    const char* ext;
    const char* mime;
} MimeMapEntry;

static const MimeMapEntry MIME_MAP[] = {
    {"html",  "text/html"},
    {"css",   "text/css"},
    {"js",    "application/javascript"},
    {"json",  "application/json"},
    {"xml",   "application/xml"},
    {"png",   "image/png"},
    {"jpg",   "image/jpeg"},
    {"jpeg",  "image/jpeg"},
    {"gif",   "image/gif"},
    {"webp",  "image/webp"},
    {"svg",   "image/svg+xml"},
    {"bmp",   "image/bmp"},
    {"ico",   "image/x-icon"},
    {"txt",   "text/plain"},
    {"pdf",   "application/pdf"},
    {"doc",   "application/msword"},
    {"docx",  "application/vnd.openxmlformats-officedocument.wordprocessingml.document"},
    {"zip",   "application/zip"},
    {"tar",   "application/x-tar"},
    {"gz",    "application/gzip"},
    {"mp3",   "audio/mpeg"},
    {"mp4",   "video/mp4"},
    {"webm",  "video/webm"},
    {"woff2", "font/woff2"},
    {"ttf",   "font/ttf"},
    {NULL,    "application/octet-stream"}  /* 默认 */
};

/* ============================================================
 * 全局状态
 * ============================================================ */

static char g_storage_path[FILE_PATH_MAX] = {0};
static size_t g_storage_path_len = 0;

/* ============================================================
 * Rust FFI 函数声明 (在 server/security/ 中实现)
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

/* 从查询字符串中提取参数值 (简易解析) */
static const char* get_query_param(const char* query, const char* key)
{
    if (!query || !key) return NULL;

    size_t key_len = strlen(key);
    const char* p = query;

    while (*p) {
        /* 跳过 & 分隔符 */
        if (*p == '&') p++;

        if (strncmp(p, key, key_len) == 0 && p[key_len] == '=') {
            return p + key_len + 1;
        }

        /* 跳到下一个参数 */
        while (*p && *p != '&') p++;
    }

    return NULL;
}

/* 从 URL 路径中提取查询参数值 */
static char* extract_query_value(const char* query, const char* key)
{
    const char* start = get_query_param(query, key);
    if (!start) return NULL;

    /* 提取值直到 & 或字符串结束 */
    const char* end = start;
    while (*end && *end != '&') end++;

    size_t len = (size_t)(end - start);
    char* value = (char*)malloc(len + 1);
    if (!value) return NULL;
    memcpy(value, start, len);
    value[len] = '\0';

    return value;
}

/* 路径穿越检测 */
static int has_path_traversal(const char* path)
{
    if (!path) return 1;

    /* 检查绝对路径 */
    if (path[0] == '/' || path[0] == '\\') return 1;

    /* Windows 盘符路径 */
    if (strlen(path) >= 2 && path[1] == ':') return 1;

    /* 检查 .. 穿越 */
    if (strstr(path, "..") != NULL) return 1;

    return 0;
}

/* 获取文件扩展名 (小写) */
static const char* get_file_extension(const char* path)
{
    if (!path) return "";

    const char* dot = strrchr(path, '.');
    if (!dot || dot == path) return "";

    return dot + 1;
}

/* 检查扩展名是否在白名单中 */
static int is_extension_allowed(const char* ext)
{
    if (!ext || *ext == '\0') return 0;

    for (int i = 0; ALLOWED_EXTENSIONS[i] != NULL; i++) {
        /* 大小写不敏感比较 */
        size_t j;
        for (j = 0; ext[j] && ALLOWED_EXTENSIONS[i][j]; j++) {
            char c1 = ext[j];
            char c2 = ALLOWED_EXTENSIONS[i][j];
            if (c1 >= 'A' && c1 <= 'Z') c1 += 32;
            if (c2 >= 'A' && c2 <= 'Z') c2 += 32;
            if (c1 != c2) break;
        }
        if (ext[j] == '\0' && ALLOWED_EXTENSIONS[i][j] == '\0') {
            return 1;
        }
    }

    return 0;
}

/* 根据扩展名获取 MIME 类型 */
static const char* get_mime_type(const char* ext)
{
    if (!ext) return "application/octet-stream";

    for (int i = 0; MIME_MAP[i].ext != NULL; i++) {
        size_t j;
        for (j = 0; ext[j] && MIME_MAP[i].ext[j]; j++) {
            char c1 = ext[j];
            char c2 = MIME_MAP[i].ext[j];
            if (c1 >= 'A' && c1 <= 'Z') c1 += 32;
            if (c2 >= 'A' && c2 <= 'Z') c2 += 32;
            if (c1 != c2) break;
        }
        if (ext[j] == '\0' && MIME_MAP[i].ext[j] == '\0') {
            return MIME_MAP[i].mime;
        }
    }

    return "application/octet-stream";
}

/* 生成唯一文件 ID (时间戳 + 随机数) */
static void generate_file_id(char* buf, size_t bufsize)
{
    if (!buf || bufsize < 32) return;

    time_t now = time(NULL);
    unsigned int r = (unsigned int)rand();
    snprintf(buf, bufsize, "%lx%08x", (unsigned long)now, r);
}

/* 写入文件内容 (原子写入: 先写临时文件再重命名) */
static int write_file_content(const char* filepath, const uint8_t* data, size_t len)
{
    if (!filepath || !data || len == 0) return -1;

    /* 构造临时文件名 */
    char tmp_path[FILE_PATH_MAX];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", filepath);

    FILE* fp = fopen(tmp_path, "wb");
    if (!fp) {
        LOG_ERROR("无法创建临时文件: %s", tmp_path);
        return -1;
    }

    size_t written = fwrite(data, 1, len, fp);
    fclose(fp);

    if (written != len) {
        remove(tmp_path);
        LOG_ERROR("文件写入不完整: %zu/%zu", written, len);
        return -1;
    }

    /* 重命名临时文件为目标文件 (原子操作) */
    if (rename(tmp_path, filepath) != 0) {
        remove(tmp_path);
        LOG_ERROR("重命名文件失败: %s -> %s", tmp_path, filepath);
        return -1;
    }

    return 0;
}

/* JWT 认证: 从请求中提取 Bearer token 并验证, 返回 user_id, 失败返回 0 */
static int64_t authenticate_request(const HttpRequest* req)
{
    const char* token = http_extract_bearer_token(req->headers, req->header_count);
    if (!token) {
        LOG_WARN("缺少 Authorization header");
        return 0;
    }

    char* uid_str = rust_verify_jwt(token);
    if (!uid_str) {
        LOG_WARN("JWT 验证失败");
        return 0;
    }

    int64_t user_id = 0;
    char* endptr = NULL;
    user_id = strtoll(uid_str, &endptr, 10);
    if (*endptr != '\0') {
        LOG_WARN("JWT payload 中的 user_id 格式无效: %s", uid_str);
        rust_free_string(uid_str);
        return 0;
    }

    rust_free_string(uid_str);
    return user_id;
}

/* 检测图片魔数并返回对应的扩展名 */
static const char* detect_image_type(const uint8_t* data, size_t len)
{
    if (!data || len < 4) return NULL;

    /* PNG: 89 50 4E 47 */
    if (data[0] == 0x89 && data[1] == 0x50 &&
        data[2] == 0x4E && data[3] == 0x47) {
        return "png";
    }

    /* JPEG: FF D8 FF */
    if (data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF) {
        return "jpg";
    }

    /* GIF: 47 49 46 38 */
    if (data[0] == 0x47 && data[1] == 0x49 &&
        data[2] == 0x46 && data[3] == 0x38) {
        return "gif";
    }

    /* WebP: 52 49 46 46 ... 57 45 42 50 */
    if (len >= 12 &&
        data[0] == 0x52 && data[1] == 0x49 &&
        data[2] == 0x46 && data[3] == 0x46 &&
        data[8] == 0x57 && data[9] == 0x45 &&
        data[10] == 0x42 && data[11] == 0x50) {
        return "webp";
    }

    /* BMP: 42 4D */
    if (data[0] == 0x42 && data[1] == 0x4D) {
        return "bmp";
    }

    return NULL;
}

/* ============================================================
 * 路由处理器
 * ============================================================ */

/**
 * POST /api/file/upload
 * 请求: multipart/form-data 或 raw body (文件二进制数据)
 * 支持通过查询参数 ?filename=xxx.ext 指定文件名以判断扩展名
 * 或通过 JSON body: {"filename":"xxx.ext","data":"<base64>"}
 *
 * 当前实现: raw body 模式, 文件名通过查询参数或 Content-Disposition header 获取
 */
void handle_file_upload(const HttpRequest* req, HttpResponse* resp, void* user_data)
{
    (void)user_data;
    http_response_init(resp);

    /* 1. JWT 认证 */
    int64_t user_id = authenticate_request(req);
    if (user_id == 0) {
        http_response_set_status(resp, 401, "Unauthorized");
        http_response_set_json(resp, json_build_error("请先登录"));
        return;
    }

    /* 2. 检查请求体 */
    if (!req->body || req->body_length == 0) {
        http_response_set_status(resp, 400, "Bad Request");
        http_response_set_json(resp, json_build_error("请求体不能为空"));
        return;
    }

    /* 3. 检查文件大小 */
    if (req->body_length > MAX_FILE_SIZE) {
        http_response_set_status(resp, 413, "Payload Too Large");
        http_response_set_json(resp, json_build_error("文件大小超过限制 (50MB)"));
        return;
    }

    /* 4. 获取文件名和扩展名 */
    const char* ext = "";

    /* 4a. 先从查询参数获取 */
    char* q_filename = extract_query_value(req->query, "filename");
    if (q_filename) {
        const char* q_ext = get_file_extension(q_filename);
        if (*q_ext != '\0') {
            /* 复制到静态缓冲区避免 dangling pointer */
            static char ext_buf[32];
            size_t elen = strlen(q_ext);
            if (elen >= sizeof(ext_buf)) elen = sizeof(ext_buf) - 1;
            memcpy(ext_buf, q_ext, elen);
            ext_buf[elen] = '\0';
            ext = ext_buf;
        }
        free(q_filename);
    }

    /* 4b. 尝试从 Content-Disposition header 获取 */
    if (*ext == '\0') {
        const char* cd = http_get_header_value(req->headers, req->header_count,
                                                "Content-Disposition");
        if (cd) {
            /* 查找 filename="..." */
            const char* fn_start = strstr(cd, "filename=\"");
            if (fn_start) {
                fn_start += 10;  /* 跳过 filename=" */
                const char* fn_end = strchr(fn_start, '"');
                if (fn_end && fn_end > fn_start) {
                    /* 提取扩展名 */
                    const char* dot = strrchr(fn_start, '.');
                    if (dot && dot < fn_end) {
                        static char ext_buf2[32];
                        size_t elen = (size_t)(fn_end - dot - 1);
                        if (elen >= sizeof(ext_buf2)) elen = sizeof(ext_buf2) - 1;
                        memcpy(ext_buf2, dot + 1, elen);
                        ext_buf2[elen] = '\0';
                        ext = ext_buf2;
                    }
                }
            }
        }
    }

    /* 4c. 尝试从 JSON body 中的 filename 字段获取 */
    if (*ext == '\0') {
        char* json_filename = get_json_string_field(req->body, req->body_length, "filename");
        if (json_filename) {
            const char* json_ext = get_file_extension(json_filename);
            if (*json_ext != '\0') {
                static char ext_buf3[32];
                size_t elen = strlen(json_ext);
                if (elen >= sizeof(ext_buf3)) elen = sizeof(ext_buf3) - 1;
                memcpy(ext_buf3, json_ext, elen);
                ext_buf3[elen] = '\0';
                ext = ext_buf3;
            }
            free(json_filename);
        }
    }

    /* 5. 校验扩展名 */
    if (*ext == '\0' || !is_extension_allowed(ext)) {
        http_response_set_status(resp, 400, "Bad Request");
        http_response_set_json(resp, json_build_error("不支持的文件类型"));
        return;
    }

    /* 6. 生成唯一文件 ID */
    char file_id[64];
    generate_file_id(file_id, sizeof(file_id));

    /* 7. 构造存储路径 */
    char filepath[FILE_PATH_MAX];
    int n = snprintf(filepath, sizeof(filepath), "%s/%s/%s.%s",
                     g_storage_path, UPLOADS_DIR, file_id, ext);
    if (n < 0 || (size_t)n >= sizeof(filepath)) {
        http_response_set_status(resp, 500, "Internal Server Error");
        http_response_set_json(resp, json_build_error("文件路径过长"));
        return;
    }

    /* 8. 写入文件 */
    if (write_file_content(filepath, req->body, req->body_length) != 0) {
        http_response_set_status(resp, 500, "Internal Server Error");
        http_response_set_json(resp, json_build_error("文件写入失败"));
        return;
    }

    LOG_INFO("文件上传成功: user_id=%lld, file=%s, size=%zu",
             (long long)user_id, filepath, req->body_length);

    /* 9. 构造响应 URL */
    char url[FILE_PATH_MAX];
    snprintf(url, sizeof(url), "/api/file/%s/%s.%s", UPLOADS_DIR, file_id, ext);

    /* 10. 返回 JSON */
    size_t json_len = 256 + strlen(url);
    char* json = (char*)malloc(json_len);
    if (!json) {
        http_response_set_status(resp, 500, "Internal Server Error");
        http_response_set_json(resp, json_build_error("内存分配失败"));
        return;
    }

    char* safe_url = json_escape_string(url);
    snprintf(json, json_len,
             "{"
             "\"status\":\"ok\","
             "\"message\":\"上传成功\","
             "\"data\":{"
             "\"file_id\":\"%s\","
             "\"url\":\"%s\","
             "\"size\":%zu"
             "}"
             "}",
             file_id,
             safe_url ? safe_url : url,
             req->body_length);

    free(safe_url);
    http_response_set_status(resp, 200, "OK");
    http_response_set_json(resp, json);
    free(json);
}

/**
 * GET /api/file/... (通配符路径 /api/file/*)
 * 静态文件服务
 * 路径格式: /api/file/uploads/{file_id}.{ext}
 *           /api/file/avatars/{user_id}.{ext}
 */
void handle_static_file(const HttpRequest* req, HttpResponse* resp, void* user_data)
{
    (void)user_data;
    http_response_init(resp);

    /* 1. 提取相对路径 (去掉 /api/file/ 前缀) */
    const char* prefix = "/api/file/";
    size_t prefix_len = strlen(prefix);
    const char* rel_path = req->path;

    if (strncmp(rel_path, prefix, prefix_len) == 0) {
        rel_path += prefix_len;
    } else {
        /* 尝试其他前缀 */
        prefix = "/api/file";
        prefix_len = strlen(prefix);
        if (strncmp(rel_path, prefix, prefix_len) == 0) {
            rel_path += prefix_len;
            if (*rel_path == '/') rel_path++;
        }
    }

    /* 2. 空路径检查 */
    if (*rel_path == '\0') {
        http_response_set_status(resp, 400, "Bad Request");
        http_response_set_json(resp, json_build_error("文件路径不能为空"));
        return;
    }

    /* 3. 路径穿越检测 */
    if (has_path_traversal(rel_path)) {
        LOG_WARN("路径穿越攻击被阻止: %s", rel_path);
        http_response_set_status(resp, 403, "Forbidden");
        http_response_set_json(resp, json_build_error("非法路径"));
        return;
    }

    /* 4. 构造完整文件路径 */
    char filepath[FILE_PATH_MAX];
    int n = snprintf(filepath, sizeof(filepath), "%s/%s",
                     g_storage_path, rel_path);
    if (n < 0 || (size_t)n >= sizeof(filepath)) {
        http_response_set_status(resp, 500, "Internal Server Error");
        http_response_set_json(resp, json_build_error("文件路径过长"));
        return;
    }

    /* 5. 获取扩展名和 MIME 类型 */
    const char* ext = get_file_extension(rel_path);
    const char* mime = get_mime_type(ext);

    /* 6. 使用 http_response_set_file 提供文件 */
    http_response_set_file(resp, filepath, mime);

    LOG_DEBUG("静态文件请求: %s -> %s (MIME: %s)", req->path, filepath, mime);
}

/**
 * POST /api/avatar/upload
 * 上传用户头像
 * Body: raw image binary data
 * 或 JSON: {"data":"<base64>"} (暂未实现 base64 解码)
 */
void handle_avatar_upload(const HttpRequest* req, HttpResponse* resp, void* user_data)
{
    (void)user_data;
    http_response_init(resp);

    /* 1. JWT 认证 */
    int64_t user_id = authenticate_request(req);
    if (user_id == 0) {
        http_response_set_status(resp, 401, "Unauthorized");
        http_response_set_json(resp, json_build_error("请先登录"));
        return;
    }

    /* 2. 检查请求体 */
    if (!req->body || req->body_length == 0) {
        http_response_set_status(resp, 400, "Bad Request");
        http_response_set_json(resp, json_build_error("请求体不能为空"));
        return;
    }

    /* 3. 检查文件大小 (头像限制 5MB) */
    if (req->body_length > 5 * 1024 * 1024) {
        http_response_set_status(resp, 413, "Payload Too Large");
        http_response_set_json(resp, json_build_error("头像文件大小超过限制 (5MB)"));
        return;
    }

    /* 4. 检测图片类型 */
    const char* img_ext = detect_image_type(req->body, req->body_length);
    if (!img_ext) {
        http_response_set_status(resp, 400, "Bad Request");
        http_response_set_json(resp, json_build_error("不支持的头像格式 (仅支持 PNG/JPEG/GIF/WebP/BMP)"));
        return;
    }

    /* 5. 构造头像存储路径 */
    char avatar_path[FILE_PATH_MAX];
    int n = snprintf(avatar_path, sizeof(avatar_path), "%s/%s/%lld.%s",
                     g_storage_path, AVATARS_DIR,
                     (long long)user_id, img_ext);
    if (n < 0 || (size_t)n >= sizeof(avatar_path)) {
        http_response_set_status(resp, 500, "Internal Server Error");
        http_response_set_json(resp, json_build_error("文件路径过长"));
        return;
    }

    /* 6. 删除旧的同名头像 (不同扩展名) */
    /* 检查并删除旧格式 */
    static const char* old_exts[] = {"png", "jpg", "jpeg", "gif", "webp", "bmp", NULL};
    for (int i = 0; old_exts[i] != NULL; i++) {
        if (strcmp(old_exts[i], img_ext) == 0) continue;  /* 跳过当前格式 */

        char old_path[FILE_PATH_MAX];
        snprintf(old_path, sizeof(old_path), "%s/%s/%lld.%s",
                 g_storage_path, AVATARS_DIR,
                 (long long)user_id, old_exts[i]);

        FILE* test = fopen(old_path, "rb");
        if (test) {
            fclose(test);
            remove(old_path);
            LOG_INFO("删除旧头像: %s", old_path);
        }
    }

    /* 7. 写入头像文件 */
    if (write_file_content(avatar_path, req->body, req->body_length) != 0) {
        http_response_set_status(resp, 500, "Internal Server Error");
        http_response_set_json(resp, json_build_error("头像文件写入失败"));
        return;
    }

    LOG_INFO("头像上传成功: user_id=%lld, format=%s, size=%zu",
             (long long)user_id, img_ext, req->body_length);

    /* 8. 构造头像 URL */
    char avatar_url[FILE_PATH_MAX];
    snprintf(avatar_url, sizeof(avatar_url), "/api/file/%s/%lld.%s",
             AVATARS_DIR, (long long)user_id, img_ext);

    /* 9. 更新用户资料中的头像 URL */
    if (db_update_user_profile(user_id, NULL, avatar_url) != 0) {
        LOG_WARN("更新用户头像 URL 失败: user_id=%lld", (long long)user_id);
    }

    /* 10. 返回 JSON */
    size_t json_len = 256 + strlen(avatar_url);
    char* json = (char*)malloc(json_len);
    if (!json) {
        http_response_set_status(resp, 500, "Internal Server Error");
        http_response_set_json(resp, json_build_error("内存分配失败"));
        return;
    }

    char* safe_url = json_escape_string(avatar_url);
    snprintf(json, json_len,
             "{"
             "\"status\":\"ok\","
             "\"message\":\"头像上传成功\","
             "\"data\":{"
             "\"avatar_url\":\"%s\","
             "\"size\":%zu"
             "}"
             "}",
             safe_url ? safe_url : avatar_url,
             req->body_length);

    free(safe_url);
    http_response_set_status(resp, 200, "OK");
    http_response_set_json(resp, json);
    free(json);
}

/**
 * 文件存储初始化
 * 创建必要的目录结构
 */
int file_storage_init(const char* base_path)
{
    if (!base_path || *base_path == '\0') {
        LOG_ERROR("文件存储路径为空");
        return -1;
    }

    /* 存储基础路径 */
    size_t len = strlen(base_path);
    if (len >= sizeof(g_storage_path)) {
        LOG_ERROR("存储路径过长");
        return -1;
    }
    memcpy(g_storage_path, base_path, len + 1);
    g_storage_path_len = len;

    LOG_INFO("初始化文件存储: %s", g_storage_path);

    /* 创建基础目录 */
    if (mkdir_p(g_storage_path) != 0) {
        /* EEXIST 是正常的 */
    }

    /* 创建 uploads 子目录 */
    char uploads_path[FILE_PATH_MAX];
    snprintf(uploads_path, sizeof(uploads_path), "%s/%s", g_storage_path, UPLOADS_DIR);
    if (mkdir_p(uploads_path) != 0) {
        /* EEXIST 是正常的 */
    }

    /* 创建 avatars 子目录 */
    char avatars_path[FILE_PATH_MAX];
    snprintf(avatars_path, sizeof(avatars_path), "%s/%s", g_storage_path, AVATARS_DIR);
    if (mkdir_p(avatars_path) != 0) {
        /* EEXIST 是正常的 */
    }

    LOG_INFO("文件存储初始化完成: %s", g_storage_path);
    return 0;
}
