/**
 * Chrono-shift 文件存储 HTTP 处理器 (骨架)
 * 语言标准: C99
 */

#include "file_handler.h"
#include "server.h"
#include "json_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void handle_file_upload(const HttpRequest* req, HttpResponse* resp, void* user_data)
{
    (void)req;
    (void)user_data;
    http_response_init(resp);
    http_response_set_json(resp, json_build_error("功能开发中 - Phase 5"));
}

void handle_file_download(const HttpRequest* req, HttpResponse* resp, void* user_data)
{
    (void)req;
    (void)user_data;
    http_response_init(resp);
    http_response_set_json(resp, json_build_error("功能开发中 - Phase 5"));
}

void handle_avatar_upload(const HttpRequest* req, HttpResponse* resp, void* user_data)
{
    (void)req;
    (void)user_data;
    http_response_init(resp);
    http_response_set_json(resp, json_build_error("功能开发中 - Phase 3"));
}

void handle_static_file(const HttpRequest* req, HttpResponse* resp, void* user_data)
{
    (void)req;
    (void)user_data;
    http_response_init(resp);
    http_response_set_json(resp, json_build_error("功能开发中 - Phase 2"));
}

int file_storage_init(const char* base_path)
{
    LOG_INFO("初始化文件存储: %s", base_path);
    (void)base_path;
    return 0;
}
