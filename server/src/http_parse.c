/**
 * Chrono-shift HTTP 解析与路由模块
 * 语言标准: C99
 *
 * HTTP 请求解析、路由匹配、响应消息构建。
 * 同时提供 HTTP 头部值提取和 Bearer token 提取的公共 API。
 */

#include "http_server.h"
#include "http_core.h"
#include "json_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/* ============================================================
 * HTTP 请求解析
 * ============================================================ */

int parse_http_request(Connection* conn)
{
    char* buf = (char*)conn->read_buf;
    size_t len = conn->read_offset;

    char* line_end = strstr(buf, "\r\n");
    if (!line_end) return -1;

    char method[16], path[2048], version[16];
    if (sscanf(buf, "%15s %2047s %15s", method, path, version) < 3)
        return -1;

    strncpy(conn->request.method_str, method, sizeof(conn->request.method_str) - 1);
    strncpy(conn->request.path, path, sizeof(conn->request.path) - 1);
    strncpy(conn->request.version, version, sizeof(conn->request.version) - 1);

    if (strcmp(method, "GET") == 0) conn->request.method = HTTP_GET;
    else if (strcmp(method, "POST") == 0) conn->request.method = HTTP_POST;
    else if (strcmp(method, "PUT") == 0) conn->request.method = HTTP_PUT;
    else if (strcmp(method, "DELETE") == 0) conn->request.method = HTTP_DELETE;
    else if (strcmp(method, "PATCH") == 0) conn->request.method = HTTP_PATCH;
    else conn->request.method = HTTP_UNKNOWN;

    char* query_start = strchr(path, '?');
    if (query_start) {
        *query_start = '\0';
        strncpy(conn->request.query, query_start + 1, sizeof(conn->request.query) - 1);
        strncpy(conn->request.path, path, sizeof(conn->request.path) - 1);
    }

    char* header_start = line_end + 2;
    char* header_end = strstr(header_start, "\r\n\r\n");
    if (!header_end) return -1;

    size_t hc = 0;
    char* line = header_start;
    while (line < header_end && hc < 64) {
        char* crlf = strstr(line, "\r\n");
        if (!crlf) break;
        char* colon = strchr(line, ':');
        if (colon && colon < crlf) {
            size_t klen = (size_t)(colon - line);
            size_t vlen = (size_t)(crlf - colon - 2);
            if (klen < 1024 && vlen < 1024) {
                strncpy(conn->request.headers[hc][0], line, klen);
                conn->request.headers[hc][0][klen] = '\0';
                const char* vs = colon + 1;
                while (*vs == ' ') vs++;
                strncpy(conn->request.headers[hc][1], vs, vlen);
                conn->request.headers[hc][1][vlen] = '\0';
                hc++;
            }
        }
        line = crlf + 2;
    }
    conn->request.header_count = hc;

    const char* body_start = header_end + 4;
    size_t body_len = len - (size_t)(body_start - buf);
    if (body_len > 0) {
        for (size_t i = 0; i < hc; i++) {
            if (strcasecmp(conn->request.headers[i][0], "Content-Length") == 0) {
                size_t cl = (size_t)atol(conn->request.headers[i][1]);
                if (body_len < cl) return -1;
                break;
            }
        }
        conn->request.body = (uint8_t*)body_start;
        conn->request.body_length = body_len;
    }

    return 0;
}

/* ============================================================
 * 构建 HTTP 响应
 * ============================================================ */

/* 安全地追加 snprintf 到缓冲区 (返回 0=成功, -1=缓冲区满) */
static int safe_append(char* buf, size_t buf_size, size_t* off, const char* fmt, ...)
{
    if (*off >= buf_size) return -1;
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(buf + *off, buf_size - *off, fmt, args);
    va_end(args);
    if (n < 0 || (size_t)n >= buf_size - *off) {
        /* 缓冲区已满, 标记但不报错 */
        *off = buf_size;
        return -1;
    }
    *off += (size_t)n;
    return 0;
}

int build_http_response(Connection* conn)
{
    HttpResponse* resp = &conn->response;
    char* buf = (char*)conn->write_buf;
    size_t off = 0;

    if (resp->status_text[0] == '\0')
        strncpy(resp->status_text, "OK", sizeof(resp->status_text) - 1);

    safe_append(buf, MAX_BUFFER_SIZE, &off,
                "HTTP/1.1 %d %s\r\n", resp->status_code, resp->status_text);

    if (resp->header_count == 0) {
        safe_append(buf, MAX_BUFFER_SIZE, &off,
                    "Content-Type: application/json; charset=utf-8\r\n");
    }

    for (size_t i = 0; i < resp->header_count; i++) {
        safe_append(buf, MAX_BUFFER_SIZE, &off,
                    "%s: %s\r\n", resp->headers[i][0], resp->headers[i][1]);
    }

    safe_append(buf, MAX_BUFFER_SIZE, &off,
                "Content-Length: %zu\r\n", resp->body_length);
    safe_append(buf, MAX_BUFFER_SIZE, &off,
                "Connection: keep-alive\r\n");
    /* ---- 安全响应头 ---- */
    safe_append(buf, MAX_BUFFER_SIZE, &off,
                "Strict-Transport-Security: max-age=31536000; includeSubDomains\r\n");
    safe_append(buf, MAX_BUFFER_SIZE, &off,
                "X-Content-Type-Options: nosniff\r\n");
    safe_append(buf, MAX_BUFFER_SIZE, &off,
                "X-Frame-Options: DENY\r\n");
    safe_append(buf, MAX_BUFFER_SIZE, &off,
                "Content-Security-Policy: default-src 'self'; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline'\r\n");
    safe_append(buf, MAX_BUFFER_SIZE, &off,
                "Referrer-Policy: no-referrer\r\n");
    safe_append(buf, MAX_BUFFER_SIZE, &off, "\r\n");

    if (resp->body && resp->body_length > 0) {
        if (off + resp->body_length <= MAX_BUFFER_SIZE) {
            memcpy(buf + off, resp->body, resp->body_length);
            off += resp->body_length;
        }
    }

    conn->write_total = off;
    return 0;
}

/* ============================================================
 * 路由查找
 * ============================================================ */

int find_route(const char* method, const char* path, RouteEntry** entry)
{
    for (size_t i = 0; i < s_ctx.route_count; i++) {
        if (strcmp(s_ctx.routes[i].method, method) != 0) continue;
        if (strcmp(s_ctx.routes[i].path, path) == 0) {
            *entry = &s_ctx.routes[i];
            return 0;
        }
        size_t rlen = strlen(s_ctx.routes[i].path);
        if (rlen > 2 && s_ctx.routes[i].path[rlen - 1] == '*' &&
            s_ctx.routes[i].path[rlen - 2] == '/') {
            if (strncmp(s_ctx.routes[i].path, path, rlen - 2) == 0) {
                *entry = &s_ctx.routes[i];
                return 0;
            }
        }
    }
    return -1;
}

/* ============================================================
 * HTTP 头部辅助函数 (公开 API)
 * ============================================================ */

const char* http_get_header_value(const char headers[64][2][1024], size_t header_count,
                                   const char* key)
{
    if (!key || header_count == 0) return NULL;
    for (size_t i = 0; i < header_count; i++) {
        if (strcasecmp(headers[i][0], key) == 0)
            return headers[i][1];
    }
    return NULL;
}

const char* http_extract_bearer_token(const char headers[64][2][1024], size_t header_count)
{
    const char* auth = http_get_header_value(headers, header_count, "Authorization");
    if (!auth) return NULL;
    const char* prefix = "Bearer ";
    size_t plen = strlen(prefix);
    if (strncasecmp(auth, prefix, plen) == 0)
        return auth + plen;
    return NULL;
}
