/**
 * Chrono-shift HTTP 服务器 (epoll/poll 事件驱动完整实现)
 * 语言标准: C99
 * 平台: Linux (epoll) / Windows (WSAPoll)
 *
 * Linux: epoll 边缘触发模式 + 工作线程池
 * Windows: WSAPoll 轮询模式
 */

#include "http_server.h"
#include "platform_compat.h"
#include "tls_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <errno.h>

/* ============================================================
 * 平台特定头文件
 * ============================================================ */
#ifdef PLATFORM_LINUX
    #include <sys/epoll.h>
    #include <pthread.h>
    #define SOCK_ERR_EAGAIN EAGAIN
#else
    #include <winsock2.h>
    #include <windows.h>
    #define SOCK_ERR_EAGAIN WSAEWOULDBLOCK
#endif

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

static ServerContext s_ctx;
static bool s_initialized = false;

/* ============================================================
 * 前向声明
 * ============================================================ */
static void  event_loop(void);
static int   handle_accept(void);
static int   handle_conn_read(Connection* conn);
static int   handle_conn_write(Connection* conn);
static int   parse_http_request(Connection* conn);
static int   build_http_response(Connection* conn);
static int   find_route(const char* method, const char* path, RouteEntry** entry);
static Connection* conn_new(socket_t fd);
static void  conn_close(Connection* conn);
static void  conn_list_add(Connection* conn);
static void  conn_list_remove(Connection* conn);
static void  conn_timeout_check(void);

/* ============================================================
 * API 实现
 * ============================================================ */

int http_server_init(const ServerConfig* config)
{
    if (net_init() != 0) {
        fprintf(stderr, "[ERROR] 网络初始化失败\n");
        return -1;
    }

    memset(&s_ctx, 0, sizeof(ServerContext));
    memcpy(&s_ctx.config, config, sizeof(ServerConfig));
    s_ctx.route_count = 0;
    s_ctx.running = false;
    s_ctx.listen_fd = INVALID_SOCKET_VAL;
    s_ctx.active_connections = 0;
    s_ctx.total_requests = 0;
    s_ctx.conn_list.next = &s_ctx.conn_list;
    s_ctx.conn_list.prev = &s_ctx.conn_list;
    s_initialized = true;

    fprintf(stdout, "[INFO] HTTP 服务器初始化完成\n");
    return 0;
}

int http_server_register_route(const char* method, const char* path,
                                RouteHandler handler, void* user_data)
{
    if (!s_initialized || s_ctx.route_count >= MAX_ROUTES) return -1;

    RouteEntry* entry = &s_ctx.routes[s_ctx.route_count++];
    strncpy(entry->method, method, sizeof(entry->method) - 1);
    strncpy(entry->path, path, sizeof(entry->path) - 1);
    entry->handler = handler;
    entry->user_data = user_data;

    fprintf(stdout, "[INFO] 注册路由: %s %s\n", method, path);
    return 0;
}

int http_server_start(void)
{
    if (!s_initialized) {
        fprintf(stderr, "[ERROR] HTTP 服务器未初始化\n");
        return -1;
    }

    /* 创建监听 socket */
    s_ctx.listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (s_ctx.listen_fd == INVALID_SOCKET_VAL) {
        fprintf(stderr, "[ERROR] 创建监听 socket 失败\n");
        return -1;
    }

    int opt = 1;
    setsockopt(s_ctx.listen_fd, SOL_SOCKET, SO_REUSEADDR,
               (const char*)&opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(s_ctx.config.host);
    addr.sin_port = htons(s_ctx.config.port);

    if (bind(s_ctx.listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "[ERROR] 绑定端口 %d 失败\n", s_ctx.config.port);
        close_socket(s_ctx.listen_fd);
        return -1;
    }

    if (listen(s_ctx.listen_fd, LISTEN_BACKLOG) < 0) {
        fprintf(stderr, "[ERROR] 监听失败\n");
        close_socket(s_ctx.listen_fd);
        return -1;
    }

    set_nonblocking(s_ctx.listen_fd);

#ifdef PLATFORM_LINUX
    /* epoll 初始化 */
    s_ctx.epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (s_ctx.epoll_fd < 0) {
        fprintf(stderr, "[ERROR] epoll_create 失败\n");
        close_socket(s_ctx.listen_fd);
        return -1;
    }
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = s_ctx.listen_fd;
    epoll_ctl(s_ctx.epoll_fd, EPOLL_CTL_ADD, s_ctx.listen_fd, &ev);
#endif

    /* 启动 */
    s_ctx.running = true;
    s_ctx.start_time_ms = now_ms();
    s_ctx.worker_count = 4;

    /* 启动工作线程 */
    for (int i = 0; i < s_ctx.worker_count; i++) {
#ifdef PLATFORM_LINUX
        pthread_create(&s_ctx.worker_threads[i], NULL,
                       (void* (*)(void*))event_loop, NULL);
#else
        s_ctx.worker_threads[i] = CreateThread(NULL, 0,
            (LPTHREAD_START_ROUTINE)event_loop, NULL, 0, NULL);
#endif
    }

    fprintf(stdout, "[INFO] HTTP 服务器已启动: %s:%d, 工作线程: %d\n",
            s_ctx.config.host, s_ctx.config.port, s_ctx.worker_count);
    return 0;
}

void http_server_stop(void)
{
    s_ctx.running = false;

    if (s_ctx.listen_fd != INVALID_SOCKET_VAL) {
        close_socket(s_ctx.listen_fd);
        s_ctx.listen_fd = INVALID_SOCKET_VAL;
    }

    /* 等待工作线程 */
    for (int i = 0; i < s_ctx.worker_count; i++) {
#ifdef PLATFORM_LINUX
        pthread_join(s_ctx.worker_threads[i], NULL);
#else
        WaitForSingleObject(s_ctx.worker_threads[i], 3000);
        CloseHandle(s_ctx.worker_threads[i]);
#endif
    }

    /* 关闭所有连接 */
    Connection* c = s_ctx.conn_list.next;
    while (c != &s_ctx.conn_list) {
        Connection* next = c->next;
        conn_close(c);
        c = next;
    }

#ifdef PLATFORM_LINUX
    if (s_ctx.epoll_fd >= 0) close(s_ctx.epoll_fd);
#endif

    net_cleanup();

    /* 清理 TLS (如果启用) */
    if (tls_server_is_enabled()) {
        tls_server_cleanup();
    }

    fprintf(stdout, "[INFO] HTTP 服务器已停止\n");
}

/* ============================================================
 * 主事件循环
 * ============================================================ */
static void event_loop(void)
{
#ifdef PLATFORM_LINUX
    struct epoll_event events[MAX_EVENTS];

    while (s_ctx.running) {
        int nfds = epoll_wait(s_ctx.epoll_fd, events, MAX_EVENTS, 1000);
        if (nfds < 0) {
            if (errno == EINTR) continue;
            break;
        }

        for (int i = 0; i < nfds; i++) {
            int fd = events[i].data.fd;
            uint32_t ev = events[i].events;

            if (fd == s_ctx.listen_fd) {
                handle_accept();
                continue;
            }

            if (ev & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
                /* 查找并关闭连接 */
                Connection* c = s_ctx.conn_list.next;
                while (c != &s_ctx.conn_list) {
                    if (c->fd == fd) { conn_close(c); break; }
                    c = c->next;
                }
                continue;
            }

            Connection* c = s_ctx.conn_list.next;
            while (c != &s_ctx.conn_list) {
                if (c->fd == fd) {
                    if ((ev & EPOLLIN) && c->state == CONN_READING)
                        handle_conn_read(c);
                    if ((ev & EPOLLOUT) && c->state == CONN_WRITING)
                        handle_conn_write(c);
                    break;
                }
                c = c->next;
            }
        }

        conn_timeout_check();
    }

#else /* PLATFORM_WINDOWS */
    /* Windows: 使用 select 轮询 */
    while (s_ctx.running) {
        fd_set read_fds, write_fds, err_fds;
        FD_ZERO(&read_fds);
        FD_ZERO(&write_fds);
        FD_ZERO(&err_fds);

        socket_t max_fd = s_ctx.listen_fd;
        FD_SET(s_ctx.listen_fd, &read_fds);

        Connection* c = s_ctx.conn_list.next;
        while (c != &s_ctx.conn_list) {
            if (c->fd != INVALID_SOCKET_VAL) {
                if (c->state == CONN_READING || c->state == CONN_WS_OPEN)
                    FD_SET(c->fd, &read_fds);
                if (c->state == CONN_WRITING)
                    FD_SET(c->fd, &write_fds);
                FD_SET(c->fd, &err_fds);
                if (c->fd > max_fd) max_fd = c->fd;
            }
            c = c->next;
        }

        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = POLL_INTERVAL_MS * 1000;

        int nfds = select((int)(max_fd + 1), &read_fds, &write_fds, &err_fds, &tv);
        if (nfds < 0) break;

        if (FD_ISSET(s_ctx.listen_fd, &read_fds))
            handle_accept();

        c = s_ctx.conn_list.next;
        while (c != &s_ctx.conn_list) {
            Connection* next = c->next;
            if (c->fd != INVALID_SOCKET_VAL) {
                if (FD_ISSET(c->fd, &err_fds)) {
                    conn_close(c);
                } else {
                    if (FD_ISSET(c->fd, &read_fds) && (c->state == CONN_READING || c->state == CONN_WS_OPEN))
                        handle_conn_read(c);
                    if (FD_ISSET(c->fd, &write_fds) && c->state == CONN_WRITING)
                        handle_conn_write(c);
                }
            }
            c = next;
        }

        conn_timeout_check();
    }
#endif
}

/* ============================================================
 * accept 处理
 * ============================================================ */
static int handle_accept(void)
{
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    while (1) {
        socket_t client_fd = accept(s_ctx.listen_fd,
                                     (struct sockaddr*)&client_addr, &addr_len);
        if (client_fd == INVALID_SOCKET_VAL) {
#ifdef PLATFORM_LINUX
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
#else
            if (WSAGetLastError() == WSAEWOULDBLOCK) break;
#endif
            break;
        }

        int opt = 1;
        setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY,
                   (const char*)&opt, sizeof(opt));

        /* TLS 握手需要在阻塞模式下进行, 之后再设为非阻塞 */
        if (!tls_server_is_enabled()) {
            set_nonblocking(client_fd);
        }

        Connection* conn = conn_new(client_fd);
        if (!conn) {
            close_socket(client_fd);
            continue;
        }

        /* 如果 TLS 已启用, 将连接包装为 TLS (阻塞模式完成握手) */
        if (tls_server_is_enabled()) {
            SSL* ssl = tls_server_wrap(client_fd);
            if (!ssl) {
                fprintf(stderr, "[TLS] 包装新连接失败, 拒绝连接\n");
                conn_close(conn);
                continue;
            }
            conn->ssl = (void*)ssl;
            /* TLS 握手完成后再设为非阻塞 */
            set_nonblocking(client_fd);
        }

#ifdef PLATFORM_LINUX
        struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
        ev.data.fd = client_fd;
        if (epoll_ctl(s_ctx.epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) < 0) {
            conn_close(conn);
            continue;
        }
#endif

        conn_list_add(conn);
    }

    return 0;
}

/* ============================================================
 * 读取处理
 * ============================================================ */
static int handle_conn_read(Connection* conn)
{
    if (!conn || conn->state == CONN_CLOSED) return -1;

    while (1) {
        if (conn->read_offset >= MAX_BUFFER_SIZE - 1) return -1;

        ssize_t n;
        if (conn->ssl) {
            /* TLS 加密读取 */
            n = tls_read((SSL*)conn->ssl,
                         (char*)(conn->read_buf + conn->read_offset),
                         (int)(MAX_BUFFER_SIZE - conn->read_offset - 1));
        } else {
            /* 纯文本读取 */
            n = recv(conn->fd,
                     (char*)(conn->read_buf + conn->read_offset),
                     MAX_BUFFER_SIZE - conn->read_offset - 1, 0);
        }

        if (n > 0) {
            conn->read_offset += (size_t)n;
            conn->read_buf[conn->read_offset] = '\0';
            conn->last_activity_ms = now_ms();
            continue;
        }

        if (n == 0) return -1;  /* 连接关闭 */

#ifdef PLATFORM_LINUX
        if (errno == EAGAIN || errno == EWOULDBLOCK) break;
#else
        if (WSAGetLastError() == WSAEWOULDBLOCK) break;
#endif
        return -1;
    }

    /* 尝试解析 HTTP */
    if (conn->read_offset > 0 && !conn->request_parsed) {
        if (parse_http_request(conn) == 0) {
            conn->request_parsed = true;
            s_ctx.total_requests++;

            http_response_init(&conn->response);
            RouteEntry* route = NULL;
            if (find_route(conn->request.method_str, conn->request.path, &route) == 0) {
                route->handler(&conn->request, &conn->response, route->user_data);
            } else {
                http_response_set_status(&conn->response, 404, "Not Found");
                http_response_set_json(&conn->response, "{\"status\":\"error\",\"message\":\"Not Found\"}");
            }

            build_http_response(conn);
            conn->state = CONN_WRITING;
            conn->write_offset = 0;

#ifdef PLATFORM_LINUX
            struct epoll_event ev;
            ev.events = EPOLLOUT | EPOLLET;
            ev.data.fd = conn->fd;
            epoll_ctl(s_ctx.epoll_fd, EPOLL_CTL_MOD, conn->fd, &ev);
#endif
            return handle_conn_write(conn);
        }
    }

    return 0;
}

/* ============================================================
 * 写入处理
 * ============================================================ */
static int handle_conn_write(Connection* conn)
{
    if (!conn || conn->state == CONN_CLOSED) return -1;

    while (conn->write_offset < conn->write_total) {
        ssize_t n;
        if (conn->ssl) {
            /* TLS 加密写入 */
            n = tls_write((SSL*)conn->ssl,
                          (const char*)(conn->write_buf + conn->write_offset),
                          (int)(conn->write_total - conn->write_offset));
        } else {
            /* 纯文本写入 */
            n = send(conn->fd,
                     (const char*)(conn->write_buf + conn->write_offset),
                     conn->write_total - conn->write_offset, 0);
        }
        if (n > 0) {
            conn->write_offset += (size_t)n;
            continue;
        }
        if (n == 0) return -1;
#ifdef PLATFORM_LINUX
        if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
#else
        if (WSAGetLastError() == WSAEWOULDBLOCK) return 0;
#endif
        return -1;
    }

    if (conn->write_offset >= conn->write_total) {
        if (conn->is_websocket) {
            conn->state = CONN_WS_OPEN;
#ifdef PLATFORM_LINUX
            struct epoll_event ev;
            ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
            ev.data.fd = conn->fd;
            epoll_ctl(s_ctx.epoll_fd, EPOLL_CTL_MOD, conn->fd, &ev);
#endif
        } else {
            if (conn->response.body &&
                conn->response.body != conn->read_buf &&
                conn->response.body != conn->write_buf) {
                free(conn->response.body);
                conn->response.body = NULL;
            }
            conn_close(conn);
        }
    }

    return 0;
}

/* ============================================================
 * HTTP 请求解析
 * ============================================================ */
static int parse_http_request(Connection* conn)
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
static int build_http_response(Connection* conn)
{
    HttpResponse* resp = &conn->response;
    char* buf = (char*)conn->write_buf;
    size_t off = 0;

    if (resp->status_text[0] == '\0')
        strncpy(resp->status_text, "OK", sizeof(resp->status_text) - 1);

    off += (size_t)snprintf(buf + off, MAX_BUFFER_SIZE - off,
                    "HTTP/1.1 %d %s\r\n", resp->status_code, resp->status_text);

    if (resp->header_count == 0) {
        off += (size_t)snprintf(buf + off, MAX_BUFFER_SIZE - off,
                        "Content-Type: application/json; charset=utf-8\r\n");
    }

    for (size_t i = 0; i < resp->header_count; i++) {
        off += (size_t)snprintf(buf + off, MAX_BUFFER_SIZE - off,
                        "%s: %s\r\n", resp->headers[i][0], resp->headers[i][1]);
    }

    off += (size_t)snprintf(buf + off, MAX_BUFFER_SIZE - off,
                    "Content-Length: %zu\r\n", resp->body_length);
    off += (size_t)snprintf(buf + off, MAX_BUFFER_SIZE - off,
                    "Connection: keep-alive\r\n");
    off += (size_t)snprintf(buf + off, MAX_BUFFER_SIZE - off, "\r\n");

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
static int find_route(const char* method, const char* path, RouteEntry** entry)
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
 * 连接管理
 * ============================================================ */
static Connection* conn_new(socket_t fd)
{
    Connection* c = (Connection*)calloc(1, sizeof(Connection));
    if (!c) return NULL;
    c->fd = fd;
    c->state = CONN_READING;
    c->ssl = NULL;                 /* 默认纯文本 */
    c->last_activity_ms = now_ms();
    return c;
}

static void conn_close(Connection* c)
{
    if (!c) return;

    /* 关闭 TLS 连接 (如果存在) */
    if (c->ssl) {
        tls_close((SSL*)c->ssl);
        c->ssl = NULL;
    }

#ifdef PLATFORM_LINUX
    if (c->fd >= 0)
        epoll_ctl(s_ctx.epoll_fd, EPOLL_CTL_DEL, c->fd, NULL);
#endif

    if (c->fd != INVALID_SOCKET_VAL) {
        close_socket(c->fd);
        c->fd = INVALID_SOCKET_VAL;
    }

    if (c->response.body &&
        c->response.body != c->read_buf &&
        c->response.body != c->write_buf) {
        free(c->response.body);
        c->response.body = NULL;
    }

    conn_list_remove(c);
    free(c);
}

static void conn_list_add(Connection* c)
{
    if (!c) return;
    c->next = s_ctx.conn_list.next;
    c->prev = &s_ctx.conn_list;
    s_ctx.conn_list.next->prev = c;
    s_ctx.conn_list.next = c;
    s_ctx.active_connections++;
}

static void conn_list_remove(Connection* c)
{
    if (!c || !c->prev) return;
    c->prev->next = c->next;
    c->next->prev = c->prev;
    c->prev = NULL;
    c->next = NULL;
    s_ctx.active_connections--;
}

static void conn_timeout_check(void)
{
    uint64_t now = now_ms();
    Connection* c = s_ctx.conn_list.next;
    while (c != &s_ctx.conn_list) {
        Connection* next = c->next;
        if (c->state != CONN_WS_OPEN &&
            (now - c->last_activity_ms) > TIMEOUT_MS) {
            conn_close(c);
        }
        c = next;
    }
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

/* ============================================================
 * 响应构建辅助函数
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
        http_response_set_json(resp, "{\"status\":\"error\",\"message\":\"File not found\"}");
        return;
    }
    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (fsize <= 0) {
        fclose(fp);
        http_response_set_status(resp, 500, "Internal Server Error");
        http_response_set_json(resp, "{\"status\":\"error\",\"message\":\"Empty file\"}");
        return;
    }
    uint8_t* buf = (uint8_t*)malloc((size_t)fsize);
    if (!buf) {
        fclose(fp);
        http_response_set_status(resp, 500, "Internal Server Error");
        http_response_set_json(resp, "{\"status\":\"error\",\"message\":\"Memory allocation failed\"}");
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
