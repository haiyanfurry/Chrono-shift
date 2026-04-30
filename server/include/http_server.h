#ifndef CHRONO_HTTP_SERVER_H
#define CHRONO_HTTP_SERVER_H

#include "server.h"
#include <stdint.h>

/* ============================================================
 * HTTP 服务器 (基于 Windows IOCP)
 * ============================================================ */

/* --- HTTP 方法 --- */
enum HttpMethod {
    HTTP_GET,
    HTTP_POST,
    HTTP_PUT,
    HTTP_DELETE,
    HTTP_PATCH,
    HTTP_UNKNOWN
};

/* --- HTTP 请求 --- */
typedef struct {
    enum HttpMethod method;
    char method_str[16];
    char path[2048];
    char query[2048];
    char version[16];
    char headers[64][2][1024];
    size_t header_count;
    uint8_t* body;
    size_t body_length;
} HttpRequest;

/* --- HTTP 响应 --- */
typedef struct {
    int status_code;
    char status_text[64];
    char headers[64][2][1024];
    size_t header_count;
    uint8_t* body;
    size_t body_length;
} HttpResponse;

/* --- 路由处理器 --- */
typedef void (*RouteHandler)(const HttpRequest* req, HttpResponse* resp, void* user_data);

/* --- API --- */
int  http_server_init(const ServerConfig* config);
int  http_server_register_route(const char* method, const char* path, RouteHandler handler, void* user_data);
int  http_server_start(void);
void http_server_stop(void);

/* 响应构建辅助 */
void http_response_init(HttpResponse* resp);
void http_response_set_status(HttpResponse* resp, int code, const char* text);
void http_response_set_header(HttpResponse* resp, const char* key, const char* value);
void http_response_set_body(HttpResponse* resp, const uint8_t* data, size_t length);
void http_response_set_json(HttpResponse* resp, const char* json_str);
void http_response_set_file(HttpResponse* resp, const char* filepath, const char* mime_type);

#endif /* CHRONO_HTTP_SERVER_H */
