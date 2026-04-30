/**
 * Chrono-shift HTTP 连接管理模块
 * 语言标准: C99
 *
 * 管理 HTTP 连接的创建、读写、关闭和超时检查。
 * 包含跨平台事件循环 (Linux epoll / Windows select)。
 */

#include "http_server.h"
#include "http_core.h"
#include "platform_compat.h"
#include "tls_server.h"
#include "json_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

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
 * 连接管理 (now_ms 由 platform_compat.h 声明)
 * ============================================================ */

Connection* conn_new(socket_t fd)
{
    Connection* c = (Connection*)calloc(1, sizeof(Connection));
    if (!c) return NULL;
    c->fd = fd;
    c->state = CONN_READING;
    c->ssl = NULL;
    c->last_activity_ms = now_ms();
    return c;
}

void conn_close(Connection* c)
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

void conn_list_add(Connection* c)
{
    if (!c) return;
    c->next = s_ctx.conn_list.next;
    c->prev = &s_ctx.conn_list;
    s_ctx.conn_list.next->prev = c;
    s_ctx.conn_list.next = c;
    s_ctx.active_connections++;
}

void conn_list_remove(Connection* c)
{
    if (!c || !c->prev) return;
    c->prev->next = c->next;
    c->next->prev = c->prev;
    c->prev = NULL;
    c->next = NULL;
    s_ctx.active_connections--;
}

void conn_timeout_check(void)
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
 * accept 处理
 * ============================================================ */

int handle_accept(void)
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

        /* TLS 强制: 所有连接必须通过 TLS 握手 (阻塞模式), 之后再设为非阻塞 */
        Connection* conn = conn_new(client_fd);
        if (!conn) {
            close_socket(client_fd);
            continue;
        }

        SSL* ssl = tls_server_wrap(client_fd);
        if (!ssl) {
            fprintf(stderr, "[TLS] 包装新连接失败, 拒绝连接 (HTTPS only)\n");
            conn_close(conn);
            continue;
        }
        conn->ssl = (void*)ssl;
        /* TLS 握手完成后再设为非阻塞 */
        set_nonblocking(client_fd);

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

int handle_conn_read(Connection* conn)
{
    if (!conn || conn->state == CONN_CLOSED) return -1;

    while (1) {
        if (conn->read_offset >= MAX_BUFFER_SIZE - 1) return -1;

        ssize_t n;
        /* TLS 加密读取 (HTTPS only) */
        n = tls_read((SSL*)conn->ssl,
                     (char*)(conn->read_buf + conn->read_offset),
                     (int)(MAX_BUFFER_SIZE - conn->read_offset - 1));

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
                http_response_set_json(&conn->response, json_build_error("Not Found"));
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

int handle_conn_write(Connection* conn)
{
    if (!conn || conn->state == CONN_CLOSED) return -1;

    while (conn->write_offset < conn->write_total) {
        ssize_t n;
        /* TLS 加密写入 (HTTPS only) */
        n = tls_write((SSL*)conn->ssl,
                      (const char*)(conn->write_buf + conn->write_offset),
                      (int)(conn->write_total - conn->write_offset));
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
 * 主事件循环
 * ============================================================ */

void event_loop(void)
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
