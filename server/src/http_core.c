/**
 * Chrono-shift HTTP 服务器核心模块
 * 语言标准: C99
 *
 * 服务器生命周期管理：初始化、启动、停止、路由注册。
 * 定义全局服务器上下文 s_ctx。
 */

#include "http_server.h"
#include "http_core.h"
#include "platform_compat.h"
#include "tls_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

/* 平台特定头文件 */
#ifdef PLATFORM_LINUX
    #include <sys/epoll.h>
    #include <pthread.h>
#endif

/* ============================================================
 * 全局状态
 * ============================================================ */
ServerContext s_ctx;
bool s_initialized = false;

/* 前向声明: event_loop 定义在 http_conn.c 中 */
extern void event_loop(void);

/* ============================================================
 * API 实现 (now_ms 由 platform_compat.h 声明)
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
