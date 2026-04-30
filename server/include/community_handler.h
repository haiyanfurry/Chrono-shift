#ifndef CHRONO_COMMUNITY_HANDLER_H
#define CHRONO_COMMUNITY_HANDLER_H

#include "http_server.h"

/* ============================================================
 * 社区/模板管理 HTTP 处理器
 * ============================================================ */

void handle_template_list(const HttpRequest* req, HttpResponse* resp, void* user_data);
void handle_template_upload(const HttpRequest* req, HttpResponse* resp, void* user_data);
void handle_template_download(const HttpRequest* req, HttpResponse* resp, void* user_data);
void handle_template_apply(const HttpRequest* req, HttpResponse* resp, void* user_data);
void handle_template_preview(const HttpRequest* req, HttpResponse* resp, void* user_data);

#endif /* CHRONO_COMMUNITY_HANDLER_H */
