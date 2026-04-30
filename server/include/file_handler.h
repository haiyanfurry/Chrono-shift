#ifndef CHRONO_FILE_HANDLER_H
#define CHRONO_FILE_HANDLER_H

#include "http_server.h"

/* ============================================================
 * 文件存储 HTTP 处理器
 * ============================================================ */

void handle_file_upload(const HttpRequest* req, HttpResponse* resp, void* user_data);
void handle_file_download(const HttpRequest* req, HttpResponse* resp, void* user_data);
void handle_avatar_upload(const HttpRequest* req, HttpResponse* resp, void* user_data);
void handle_static_file(const HttpRequest* req, HttpResponse* resp, void* user_data);

/* 文件存储路径初始化 */
int file_storage_init(const char* base_path);

#endif /* CHRONO_FILE_HANDLER_H */
