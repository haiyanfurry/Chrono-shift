#ifndef CHRONO_USER_HANDLER_H
#define CHRONO_USER_HANDLER_H

#include "http_server.h"

/* ============================================================
 * 用户管理 HTTP 处理器
 * ============================================================ */

void handle_user_register(const HttpRequest* req, HttpResponse* resp, void* user_data);
void handle_user_login(const HttpRequest* req, HttpResponse* resp, void* user_data);
void handle_user_profile(const HttpRequest* req, HttpResponse* resp, void* user_data);
void handle_user_update(const HttpRequest* req, HttpResponse* resp, void* user_data);
void handle_user_search(const HttpRequest* req, HttpResponse* resp, void* user_data);
void handle_user_friends(const HttpRequest* req, HttpResponse* resp, void* user_data);
void handle_user_add_friend(const HttpRequest* req, HttpResponse* resp, void* user_data);

#endif /* CHRONO_USER_HANDLER_H */
