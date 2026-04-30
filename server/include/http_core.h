#ifndef CHRONO_HTTP_CORE_H
#define CHRONO_HTTP_CORE_H

/**
 * Chrono-shift HTTP 服务器内部头文件
 * 语言标准: C99
 *
 * 暴露内部数据结构和跨模块函数声明给 http_*.c 子模块使用。
 * 此头文件不对外公开。
 */

#include "http_server.h"
#include "platform_compat.h"
#include <stdint.h>
#include <stdbool.h>

/* ============================================================
 * 常量定义
 * ============================================================ */
#define MAX_ROUTES          128
#define MAX_WORKERS         16
#define MAX_EVENTS          1024
#define MAX_BUFFER_SIZE     65536
#define LISTEN_BACKLOG      1024
#define TIMEOUT_MS          30000
#define POLL_INTERVAL_MS    50

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

/* 连接状态 */
typedef enum {
    CONN_READING,
    CONN_WRITING,
    CONN_WS_OPEN,
    CONN_CLOSED
} ConnState;

/* 每连接数据 */
typedef struct Connection {
    socket_t fd;
    ConnState state;
    uint8_t read_buf[MAX_BUFFER_SIZE];
    size_t read_offset;
    uint8_t write_buf[MAX_BUFFER_SIZE];
    size_t write_offset;
    size_t write_total;
    HttpRequest request;
    HttpResponse response;
    bool request_parsed;
    bool is_websocket;
    void* ws_conn;
    uint64_t last_activity_ms;
    struct Connection* next;
    struct Connection* prev;
    void* ssl;                     /* TLS: SSL* 对象, NULL=纯文本 */
} Connection;

/* 服务器上下文 */
typedef struct {
    socket_t listen_fd;
#ifdef PLATFORM_LINUX
    int epoll_fd;
#endif
    bool running;
    RouteEntry routes[MAX_ROUTES];
    size_t route_count;
    ServerConfig config;
    Connection conn_list;          /* 哨兵节点 */
    int active_connections;
    uint64_t total_requests;
    uint64_t start_time_ms;
#ifdef PLATFORM_LINUX
    pthread_t worker_threads[MAX_WORKERS];
#else
    HANDLE worker_threads[MAX_WORKERS];
#endif
    int worker_count;
} ServerContext;

/* 全局服务器上下文 */
extern ServerContext s_ctx;
extern bool s_initialized;

/* ============================================================
 * 跨模块内部函数声明
 * ============================================================ */

/* http_conn.c: 连接 I/O */
int  handle_accept(void);
int  handle_conn_read(Connection* conn);
int  handle_conn_write(Connection* conn);

/* http_parse.c: HTTP 解析 / 路由 */
int  parse_http_request(Connection* conn);
int  build_http_response(Connection* conn);
int  find_route(const char* method, const char* path, RouteEntry** entry);

/* http_conn.c: 连接管理 */
Connection* conn_new(socket_t fd);
void conn_close(Connection* conn);
void conn_list_add(Connection* conn);
void conn_list_remove(Connection* conn);
void conn_timeout_check(void);

#endif /* CHRONO_HTTP_CORE_H */
