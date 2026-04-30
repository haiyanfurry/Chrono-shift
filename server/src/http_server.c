/**
 * Chrono-shift HTTP 服务器 (IOCP 事件驱动完整实现)
 * 语言标准: C99
 * 平台: Windows 10/11 x64
 * 
 * 基于 Windows IOCP (I/O Completion Port) 的高性能 HTTP 服务器
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <mswsock.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "http_server.h"

/* ============================================================
 * 常量定义
 * ============================================================ */
#define MAX_ROUTES          128
#define MAX_WORKERS         16
#define MAX_BUFFER_SIZE     65536
#define MAX_HEADER_SIZE     8192
#define IOCP_BUF_COUNT      64

/* ============================================================
 * 数据结构
 * ============================================================ */

/* 路由条目 */
typedef struct {
    char method[16];
    char path[2048];
    RouteHandler handler;
    void* user_data;
} RouteEntry;

/* 每连接数据 */
typedef struct {
    OVERLAPPED overlapped;
    SOCKET socket;
    WSABUF wsa_buf;
    uint8_t buffer[MAX_BUFFER_SIZE];
    size_t recv_bytes;
    size_t send_bytes;
    bool receiving;
    bool closed;
    
    /* 请求解析状态 */
    HttpRequest request;
    HttpResponse response;
    bool request_parsed;
    bool response_sent;
    
    /* WebSocket 升级 */
    bool is_websocket;
    void* ws_conn;
} PerIoData;

/* 服务器上下文 */
typedef struct {
    SOCKET listen_socket;
    HANDLE iocp;
    HANDLE worker_threads[MAX_WORKERS];
    DWORD worker_count;
    
    RouteEntry routes[MAX_ROUTES];
    size_t route_count;
    
    ServerConfig config;
    bool running;
    
    /* AcceptEx 函数指针 */
    LPFN_ACCEPTEX lpfnAcceptEx;
    LPFN_GETACCEPTEXSOCKADDRS lpfnGetAcceptExSockaddrs;
} ServerContext;

/* ============================================================
 * 全局状态
 * ============================================================ */
static ServerContext s_ctx;
static bool s_initialized = false;

/* ============================================================
 * 前向声明
 * ============================================================ */
static DWORD WINAPI worker_thread(LPVOID param);
static void handle_io_completion(PerIoData* io_data, DWORD bytes_transferred, ULONG_PTR completion_key);
static int  parse_http_request(PerIoData* io_data);
static int  build_http_response(PerIoData* io_data);
static int  find_route(const char* method, const char* path, RouteEntry** entry);
static void begin_recv(PerIoData* io_data);
static void begin_send(PerIoData* io_data);
static PerIoData* create_io_data(void);
static void free_io_data(PerIoData* io_data);

/* ============================================================
 * API 实现
 * ============================================================ */

int http_server_init(const ServerConfig* config)
{
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        LOG_ERROR("WSAStartup 失败");
        return -1;
    }

    memset(&s_ctx, 0, sizeof(ServerContext));
    memcpy(&s_ctx.config, config, sizeof(ServerConfig));
    s_ctx.route_count = 0;
    s_ctx.running = false;
    s_initialized = true;

    LOG_INFO("HTTP 服务器 IOCP 模式初始化完成");
    return 0;
}

int http_server_register_route(const char* method, const char* path,
                                RouteHandler handler, void* user_data)
{
    if (!s_initialized || s_ctx.route_count >= MAX_ROUTES) {
        return -1;
    }

    RouteEntry* entry = &s_ctx.routes[s_ctx.route_count++];
    strncpy(entry->method, method, sizeof(entry->method) - 1);
    strncpy(entry->path, path, sizeof(entry->path) - 1);
    entry->handler = handler;
    entry->user_data = user_data;

    LOG_INFO("注册路由: %s %s", method, path);
    return 0;
}

int http_server_start(void)
{
    if (!s_initialized) {
        LOG_ERROR("HTTP 服务器未初始化");
        return -1;
    }

    /* 创建监听 socket */
    s_ctx.listen_socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, 
                                     NULL, 0, WSA_FLAG_OVERLAPPED);
    if (s_ctx.listen_socket == INVALID_SOCKET) {
        LOG_ERROR("创建监听 socket 失败: %d", WSAGetLastError());
        return -1;
    }

    /* 设置 SO_REUSEADDR */
    int opt = 1;
    setsockopt(s_ctx.listen_socket, SOL_SOCKET, SO_REUSEADDR, 
               (const char*)&opt, sizeof(opt));

    /* 绑定地址 */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(s_ctx.config.host);
    addr.sin_port = htons(s_ctx.config.port);

    if (bind(s_ctx.listen_socket, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        LOG_ERROR("绑定端口 %d 失败: %d", s_ctx.config.port, WSAGetLastError());
        closesocket(s_ctx.listen_socket);
        return -1;
    }

    /* 开始监听 */
    if (listen(s_ctx.listen_socket, SOMAXCONN) == SOCKET_ERROR) {
        LOG_ERROR("监听失败: %d", WSAGetLastError());
        closesocket(s_ctx.listen_socket);
        return -1;
    }

    /* 创建 IOCP */
    s_ctx.iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    if (!s_ctx.iocp) {
        LOG_ERROR("创建 IOCP 失败: %d", GetLastError());
        closesocket(s_ctx.listen_socket);
        return -1;
    }

    /* 将监听 socket 关联到 IOCP */
    if (CreateIoCompletionPort((HANDLE)s_ctx.listen_socket, s_ctx.iocp, 0, 0) == NULL) {
        LOG_ERROR("关联监听 socket 到 IOCP 失败: %d", GetLastError());
        CloseHandle(s_ctx.iocp);
        closesocket(s_ctx.listen_socket);
        return -1;
    }

    /* 加载 AcceptEx 函数 */
    GUID guidAcceptEx = WSAID_ACCEPTEX;
    DWORD bytes;
    WSAIoctl(s_ctx.listen_socket, SIO_GET_EXTENSION_FUNCTION_POINTER,
             &guidAcceptEx, sizeof(guidAcceptEx),
             &s_ctx.lpfnAcceptEx, sizeof(s_ctx.lpfnAcceptEx),
             &bytes, NULL, NULL);

    GUID guidGetExSockaddrs = WSAID_GETACCEPTEXSOCKADDRS;
    WSAIoctl(s_ctx.listen_socket, SIO_GET_EXTENSION_FUNCTION_POINTER,
             &guidGetExSockaddrs, sizeof(guidGetExSockaddrs),
             &s_ctx.lpfnGetAcceptExSockaddrs, sizeof(s_ctx.lpfnGetAcceptExSockaddrs),
             &bytes, NULL, NULL);

    /* 创建工作线程 */
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    s_ctx.worker_count = (DWORD)sysinfo.dwNumberOfProcessors * 2;
    if (s_ctx.worker_count > MAX_WORKERS) s_ctx.worker_count = MAX_WORKERS;
    if (s_ctx.worker_count < 2) s_ctx.worker_count = 2;

    s_ctx.running = true;

    for (DWORD i = 0; i < s_ctx.worker_count; i++) {
        s_ctx.worker_threads[i] = CreateThread(NULL, 0, worker_thread, NULL, 0, NULL);
        if (!s_ctx.worker_threads[i]) {
            LOG_ERROR("创建工作线程 %lu 失败", i);
        }
    }

    LOG_INFO("HTTP 服务器已启动: %s:%d, 工作线程: %lu", 
             s_ctx.config.host, s_ctx.config.port, s_ctx.worker_count);

    /* 提交初始 AcceptEx */
    for (DWORD i = 0; i < IOCP_BUF_COUNT; i++) {
        PerIoData* io_data = create_io_data();
        if (io_data) {
            /* 准备 AcceptEx 的临时 socket */
            SOCKET accept_sock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP,
                                           NULL, 0, WSA_FLAG_OVERLAPPED);
            if (accept_sock != INVALID_SOCKET) {
                io_data->socket = accept_sock;
                
                DWORD recv_bytes;
                BOOL result = s_ctx.lpfnAcceptEx(
                    s_ctx.listen_socket, accept_sock,
                    io_data->buffer, 0,
                    sizeof(struct sockaddr_in) + 16,
                    sizeof(struct sockaddr_in) + 16,
                    &recv_bytes,
                    &io_data->overlapped
                );
                
                if (!result && WSAGetLastError() != ERROR_IO_PENDING) {
                    LOG_WARN("AcceptEx 提交失败: %d", WSAGetLastError());
                    closesocket(accept_sock);
                    free_io_data(io_data);
                }
            } else {
                free_io_data(io_data);
            }
        }
    }

    return 0;
}

void http_server_stop(void)
{
    s_ctx.running = false;

    /* 关闭监听 socket 以唤醒工作线程 */
    if (s_ctx.listen_socket != INVALID_SOCKET) {
        closesocket(s_ctx.listen_socket);
    }

    /* 等待工作线程结束 */
    for (DWORD i = 0; i < s_ctx.worker_count; i++) {
        if (s_ctx.worker_threads[i]) {
            WaitForSingleObject(s_ctx.worker_threads[i], 3000);
            CloseHandle(s_ctx.worker_threads[i]);
        }
    }

    if (s_ctx.iocp) {
        CloseHandle(s_ctx.iocp);
    }

    WSACleanup();
    LOG_INFO("HTTP 服务器已停止");
}

/* ============================================================
 * 工作线程
 * ============================================================ */
static DWORD WINAPI worker_thread(LPVOID param)
{
    (void)param;

    while (s_ctx.running) {
        DWORD bytes_transferred;
        ULONG_PTR completion_key;
        LPOVERLAPPED overlapped = NULL;

        BOOL result = GetQueuedCompletionStatus(
            s_ctx.iocp,
            &bytes_transferred,
            &completion_key,
            &overlapped,
            INFINITE
        );

        if (!overlapped) {
            continue;
        }

        PerIoData* io_data = CONTAINING_RECORD(overlapped, PerIoData, overlapped);

        if (!result) {
            DWORD err = GetLastError();
            if (err != ERROR_OPERATION_ABORTED) {
                LOG_WARN("IOCP 错误: %lu", err);
            }
            free_io_data(io_data);
            continue;
        }

        if (completion_key == 0 && bytes_transferred == 0) {
            /* AcceptEx 完成 — 新连接 */
            struct sockaddr_in* local_addr = NULL;
            struct sockaddr_in* remote_addr = NULL;
            int local_len = sizeof(struct sockaddr_in) + 16;
            int remote_len = sizeof(struct sockaddr_in) + 16;
            
            s_ctx.lpfnGetAcceptExSockaddrs(
                io_data->buffer, 0,
                sizeof(struct sockaddr_in) + 16,
                sizeof(struct sockaddr_in) + 16,
                (struct sockaddr**)&local_addr, &local_len,
                (struct sockaddr**)&remote_addr, &remote_len
            );

            /* 设置新 socket 为非阻塞 */
            u_long nonblock = 1;
            ioctlsocket(io_data->socket, FIONBIO, &nonblock);

            /* 关联到 IOCP */
            if (CreateIoCompletionPort((HANDLE)io_data->socket, s_ctx.iocp, 1, 0) == NULL) {
                LOG_WARN("关联客户端 socket 到 IOCP 失败");
                free_io_data(io_data);
            } else {
                io_data->receiving = true;
                begin_recv(io_data);
            }

            /* 提交下一个 AcceptEx */
            if (s_ctx.running) {
                PerIoData* new_accept = create_io_data();
                if (new_accept) {
                    SOCKET new_sock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP,
                                                NULL, 0, WSA_FLAG_OVERLAPPED);
                    if (new_sock != INVALID_SOCKET) {
                        new_accept->socket = new_sock;
                        DWORD recv_bytes;
                        s_ctx.lpfnAcceptEx(
                            s_ctx.listen_socket, new_sock,
                            new_accept->buffer, 0,
                            sizeof(struct sockaddr_in) + 16,
                            sizeof(struct sockaddr_in) + 16,
                            &recv_bytes,
                            &new_accept->overlapped
                        );
                    } else {
                        free_io_data(new_accept);
                    }
                }
            }

            continue;
        }

        /* 处理现有连接的 IO 完成 */
        handle_io_completion(io_data, bytes_transferred, completion_key);
    }

    return 0;
}

/* ============================================================
 * IO 完成处理
 * ============================================================ */
static void handle_io_completion(PerIoData* io_data, DWORD bytes_transferred, ULONG_PTR completion_key)
{
    (void)completion_key;

    if (bytes_transferred == 0 || io_data->closed) {
        free_io_data(io_data);
        return;
    }

    if (io_data->receiving) {
        /* 接收完成 */
        io_data->recv_bytes += bytes_transferred;
        io_data->buffer[io_data->recv_bytes] = '\0';

        /* 解析 HTTP 请求 */
        if (parse_http_request(io_data) == 0) {
            io_data->request_parsed = true;
            
            /* 查找并执行路由处理器 */
            RouteEntry* route = NULL;
            if (find_route(io_data->request.method_str, io_data->request.path, &route) == 0) {
                http_response_init(&io_data->response);
                route->handler(&io_data->request, &io_data->response, route->user_data);
            } else {
                /* 404 */
                http_response_init(&io_data->response);
                http_response_set_status(&io_data->response, 404, "Not Found");
                char* body = "{\"status\":\"error\",\"message\":\"Not Found\"}";
                http_response_set_json(&io_data->response, body);
            }

            /* 发送响应 */
            io_data->receiving = false;
            build_http_response(io_data);
            begin_send(io_data);
        } else {
            /* 请求未完整，继续接收 */
            begin_recv(io_data);
        }
    } else {
        /* 发送完成 — 检查是否为 WebSocket */
        if (io_data->is_websocket) {
            /* WebSocket 连接继续保持 */
            io_data->receiving = true;
            begin_recv(io_data);
        } else {
            /* HTTP 短连接，关闭 */
            free_io_data(io_data);
        }
    }
}

/* ============================================================
 * HTTP 请求解析
 * ============================================================ */
static int parse_http_request(PerIoData* io_data)
{
    char* buf = (char*)io_data->buffer;
    size_t len = io_data->recv_bytes;

    /* 查找请求行结束 */
    char* line_end = strstr(buf, "\r\n");
    if (!line_end) return -1;

    /* 解析请求行: METHOD PATH HTTP/1.1 */
    char method[16], path[2048], version[16];
    if (sscanf(buf, "%15s %2047s %15s", method, path, version) < 3) {
        return -1;
    }

    strncpy(io_data->request.method_str, method, sizeof(io_data->request.method_str) - 1);
    strncpy(io_data->request.path, path, sizeof(io_data->request.path) - 1);
    strncpy(io_data->request.version, version, sizeof(io_data->request.version) - 1);

    /* 解析 HTTP 方法枚举 */
    if (strcmp(method, "GET") == 0) io_data->request.method = HTTP_GET;
    else if (strcmp(method, "POST") == 0) io_data->request.method = HTTP_POST;
    else if (strcmp(method, "PUT") == 0) io_data->request.method = HTTP_PUT;
    else if (strcmp(method, "DELETE") == 0) io_data->request.method = HTTP_DELETE;
    else if (strcmp(method, "PATCH") == 0) io_data->request.method = HTTP_PATCH;
    else io_data->request.method = HTTP_UNKNOWN;

    /* 解析查询字符串 */
    char* query_start = strchr(path, '?');
    if (query_start) {
        *query_start = '\0';
        strncpy(io_data->request.query, query_start + 1, sizeof(io_data->request.query) - 1);
        strncpy(io_data->request.path, path, sizeof(io_data->request.path) - 1);
    }

    /* 解析头部 */
    char* header_start = line_end + 2;
    char* header_end = strstr(header_start, "\r\n\r\n");
    if (!header_end) return -1;

    size_t header_count = 0;
    char* line = header_start;
    while (line < header_end && header_count < 64) {
        char* crlf = strstr(line, "\r\n");
        if (!crlf) break;

        char* colon = strchr(line, ':');
        if (colon && colon < crlf) {
            size_t key_len = colon - line;
            size_t val_len = crlf - colon - 2;
            if (key_len < 1024 && val_len < 1024) {
                strncpy(io_data->request.headers[header_count][0], line, key_len);
                io_data->request.headers[header_count][0][key_len] = '\0';
                
                /* 跳过空格 */
                const char* val_start = colon + 1;
                while (*val_start == ' ') val_start++;
                
                strncpy(io_data->request.headers[header_count][1], val_start, val_len);
                io_data->request.headers[header_count][1][val_len] = '\0';
                header_count++;
            }
        }
        line = crlf + 2;
    }
    io_data->request.header_count = header_count;

    /* 解析请求体 */
    const char* body_start = header_end + 4;
    size_t body_len = len - (body_start - buf);
    if (body_len > 0) {
        /* 查找 Content-Length */
        for (size_t i = 0; i < header_count; i++) {
            if (strcasecmp(io_data->request.headers[i][0], "Content-Length") == 0) {
                size_t content_length = (size_t)atol(io_data->request.headers[i][1]);
                if (body_len < content_length) {
                    return -1; /* 还未接收完整 */
                }
                break;
            }
        }
        io_data->request.body = (uint8_t*)body_start;
        io_data->request.body_length = body_len;
    }

    return 0;
}

/* ============================================================
 * 构建 HTTP 响应
 * ============================================================ */
static int build_http_response(PerIoData* io_data)
{
    HttpResponse* resp = &io_data->response;
    char* buf = (char*)io_data->buffer;
    size_t offset = 0;

    /* 状态行 */
    if (resp->status_text[0] == '\0') {
        strncpy(resp->status_text, "OK", sizeof(resp->status_text) - 1);
    }

    offset += snprintf(buf + offset, MAX_BUFFER_SIZE - offset,
                       "HTTP/1.1 %d %s\r\n", resp->status_code, resp->status_text);

    /* 默认头部 */
    if (resp->header_count == 0) {
        offset += snprintf(buf + offset, MAX_BUFFER_SIZE - offset,
                          "Content-Type: application/json; charset=utf-8\r\n");
    }

    /* 自定义头部 */
    for (size_t i = 0; i < resp->header_count; i++) {
        offset += snprintf(buf + offset, MAX_BUFFER_SIZE - offset,
                          "%s: %s\r\n", resp->headers[i][0], resp->headers[i][1]);
    }

    /* Content-Length */
    offset += snprintf(buf + offset, MAX_BUFFER_SIZE - offset,
                      "Content-Length: %zu\r\n", resp->body_length);

    /* Connection */
    offset += snprintf(buf + offset, MAX_BUFFER_SIZE - offset,
                      "Connection: keep-alive\r\n");

    /* 结束头部 */
    offset += snprintf(buf + offset, MAX_BUFFER_SIZE - offset, "\r\n");

    /* 响应体 */
    if (resp->body && resp->body_length > 0) {
        memcpy(buf + offset, resp->body, resp->body_length);
        offset += resp->body_length;
    }

    io_data->send_bytes = offset;
    return 0;
}

/* ============================================================
 * 收发操作
 * ============================================================ */
static void begin_recv(PerIoData* io_data)
{
    DWORD flags = 0;
    DWORD recv_bytes = 0;
    
    io_data->wsa_buf.buf = (char*)(io_data->buffer + io_data->recv_bytes);
    io_data->wsa_buf.len = MAX_BUFFER_SIZE - io_data->recv_bytes - 1;

    memset(&io_data->overlapped, 0, sizeof(OVERLAPPED));

    int result = WSARecv(io_data->socket, &io_data->wsa_buf, 1, 
                         &recv_bytes, &flags, &io_data->overlapped, NULL);

    if (result == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
        free_io_data(io_data);
    }
}

static void begin_send(PerIoData* io_data)
{
    io_data->wsa_buf.buf = (char*)io_data->buffer;
    io_data->wsa_buf.len = io_data->send_bytes;

    memset(&io_data->overlapped, 0, sizeof(OVERLAPPED));

    DWORD sent_bytes = 0;
    int result = WSASend(io_data->socket, &io_data->wsa_buf, 1,
                         &sent_bytes, 0, &io_data->overlapped, NULL);

    if (result == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
        free_io_data(io_data);
    }
}

/* ============================================================
 * 路由查找
 * ============================================================ */
static int find_route(const char* method, const char* path, RouteEntry** entry)
{
    for (size_t i = 0; i < s_ctx.route_count; i++) {
        if (strcmp(s_ctx.routes[i].method, method) != 0) continue;
        
        /* 精确匹配 */
        if (strcmp(s_ctx.routes[i].path, path) == 0) {
            *entry = &s_ctx.routes[i];
            return 0;
        }
        
        /* 通配符匹配: /api/file/* */
        size_t route_len = strlen(s_ctx.routes[i].path);
        if (route_len > 2 && s_ctx.routes[i].path[route_len - 1] == '*' &&
            s_ctx.routes[i].path[route_len - 2] == '/') {
            if (strncmp(s_ctx.routes[i].path, path, route_len - 2) == 0) {
                *entry = &s_ctx.routes[i];
                return 0;
            }
        }
    }

    return -1;
}

/* ============================================================
 * IO 数据管理
 * ============================================================ */
static PerIoData* create_io_data(void)
{
    PerIoData* io_data = (PerIoData*)calloc(1, sizeof(PerIoData));
    if (io_data) {
        io_data->socket = INVALID_SOCKET;
        memset(&io_data->overlapped, 0, sizeof(OVERLAPPED));
    }
    return io_data;
}

static void free_io_data(PerIoData* io_data)
{
    if (!io_data) return;
    
    if (io_data->socket != INVALID_SOCKET) {
        closesocket(io_data->socket);
    }

    /* 释放响应体（如果是堆分配的） */
    if (io_data->response.body && io_data->response.body != io_data->buffer) {
        free(io_data->response.body);
    }

    free(io_data);
}

/* ============================================================
 * HTTP 头部辅助函数 (声明在 server.h)
 * ============================================================ */

const char* http_get_header_value(const char headers[64][2][1024], size_t header_count,
                                   const char* key)
{
    if (!key || header_count == 0) return NULL;

    for (size_t i = 0; i < header_count; i++) {
        if (strcasecmp(headers[i][0], key) == 0) {
            return headers[i][1];
        }
    }

    return NULL;
}

const char* http_extract_bearer_token(const char headers[64][2][1024], size_t header_count)
{
    const char* auth = http_get_header_value(headers, header_count, "Authorization");
    if (!auth) return NULL;

    /* 查找 "Bearer " 前缀 */
    const char* prefix = "Bearer ";
    size_t prefix_len = strlen(prefix);

    if (strncasecmp(auth, prefix, prefix_len) == 0) {
        return auth + prefix_len;
    }

    return NULL;
}
