/**
 * Chrono-shift HTTP 响应构建模块
 * 语言标准: C99
 *
 * 提供 HttpResponse 结构体初始化和响应构建辅助函数。
 * 这些函数是公开 API，由各 handler 模块调用。
 */

#include "http_server.h"
#include "json_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * 响应初始化
 * ============================================================ */

void http_response_init(HttpResponse* resp)
{
    if (!resp) return;
    resp->status_code = 200;
    strcpy(resp->status_text, "OK");
    resp->header_count = 0;
    resp->body = NULL;
    resp->body_length = 0;
    /* 设置默认 Content-Type */
    http_response_set_header(resp, "Content-Type", "text/plain");
    http_response_set_header(resp, "Server", "Chrono-shift/1.0");
}

void http_response_set_status(HttpResponse* resp, int code, const char* text)
{
    if (!resp || !text) return;
    resp->status_code = code;
    strncpy(resp->status_text, text, sizeof(resp->status_text) - 1);
    resp->status_text[sizeof(resp->status_text) - 1] = '\0';
}

void http_response_set_header(HttpResponse* resp, const char* key, const char* value)
{
    if (!resp || !key || !value) return;
    if (resp->header_count >= 64) return;
    strncpy(resp->headers[resp->header_count][0], key, 1023);
    resp->headers[resp->header_count][0][1023] = '\0';
    strncpy(resp->headers[resp->header_count][1], value, 1023);
    resp->headers[resp->header_count][1][1023] = '\0';
    resp->header_count++;
}

void http_response_set_body(HttpResponse* resp, const uint8_t* data, size_t length)
{
    if (!resp) return;
    /* 释放旧 body */
    if (resp->body) {
        free(resp->body);
        resp->body = NULL;
    }
    if (data && length > 0) {
        resp->body = (uint8_t*)malloc(length);
        if (resp->body) {
            memcpy(resp->body, data, length);
            resp->body_length = length;
        }
    } else {
        resp->body_length = 0;
    }
}

void http_response_set_json(HttpResponse* resp, const char* json_str)
{
    if (!resp || !json_str) return;
    http_response_set_body(resp, (const uint8_t*)json_str, strlen(json_str));
    http_response_set_header(resp, "Content-Type", "application/json; charset=utf-8");
}

void http_response_set_file(HttpResponse* resp, const char* filepath, const char* mime_type)
{
    if (!resp || !filepath) return;
    FILE* fp = fopen(filepath, "rb");
    if (!fp) {
        http_response_set_status(resp, 404, "Not Found");
        http_response_set_json(resp, json_build_error("File not found"));
        return;
    }
    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (fsize <= 0) {
        fclose(fp);
        http_response_set_status(resp, 500, "Internal Server Error");
        http_response_set_json(resp, json_build_error("Empty file"));
        return;
    }
    uint8_t* buf = (uint8_t*)malloc((size_t)fsize);
    if (!buf) {
        fclose(fp);
        http_response_set_status(resp, 500, "Internal Server Error");
        http_response_set_json(resp, json_build_error("Memory allocation failed"));
        return;
    }
    size_t nread = fread(buf, 1, (size_t)fsize, fp);
    fclose(fp);
    http_response_set_body(resp, buf, nread);
    free(buf);
    if (mime_type) {
        char content_type[1024];
        snprintf(content_type, sizeof(content_type), "%s; charset=utf-8", mime_type);
        http_response_set_header(resp, "Content-Type", content_type);
    }
}
