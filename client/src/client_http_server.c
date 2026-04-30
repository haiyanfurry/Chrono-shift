/**
 * Chrono-shift 客户端本地 HTTP 服务
 * 语言标准: C99
 * 平台: Windows (WinSock2)
 *
 * 轻量级 HTTP 服务器，监听 127.0.0.1:9010
 * 为 WebView2 前端提供本地 API 桥接
 */

#include "client_http_server.h"
#include "client.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>

/* ============================================================
 * 常量定义
 * ============================================================ */

#define MAX_BUF_SIZE     8192
#define LISTEN_BACKLOG   10
#define POLL_INTERVAL_MS 100

/* ============================================================
 * 全局状态
 * ============================================================ */

static struct {
    SOCKET listen_fd;
    uint16_t port;
    volatile bool running;
    HANDLE thread;
} g_local_server = { INVALID_SOCKET, 0, false, NULL };

/* ============================================================
 * HTTP 响应辅助
 * ============================================================ */

static void send_response(SOCKET fd, int status_code, const char* status_text,
                          const char* content_type, const char* body)
{
    char header[512];
    int header_len = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "\r\n",
        status_code, status_text,
        content_type ? content_type : "text/plain",
        body ? strlen(body) : 0);

    send(fd, header, header_len, 0);
    if (body) {
        send(fd, body, strlen(body), 0);
    }
}

static void send_json_response(SOCKET fd, int status_code, const char* status_text,
                                const char* json_body)
{
    send_response(fd, status_code, status_text, "application/json", json_body);
}

static void send_error_json(SOCKET fd, int status_code, const char* message)
{
    char json[512];
    snprintf(json, sizeof(json),
             "{\"status\":\"error\",\"message\":\"%s\"}", message);
    send_json_response(fd, status_code, "Error", json);
}

/* ============================================================
 * 路由处理
 * ============================================================ */

/* GET /health — 健康检查 */
static void handle_health(SOCKET fd)
{
    const char* json = "{\"status\":\"ok\",\"service\":\"chrono-client-local\"}";
    send_json_response(fd, 200, "OK", json);
}

/* GET /api/local/status — 本地状态 */
static void handle_local_status(SOCKET fd)
{
    const char* json = "{"
        "\"status\":\"running\","
        "\"version\":\"0.1.0\""
        "}";
    send_json_response(fd, 200, "OK", json);
}

/* 404 处理 */
static void handle_not_found(SOCKET fd)
{
    send_error_json(fd, 404, "Not Found");
}

/* ============================================================
 * 请求解析（极简 HTTP/1.1 解析器）
 * ============================================================ */

static void handle_client(SOCKET fd)
{
    char buf[MAX_BUF_SIZE];
    int received = recv(fd, buf, sizeof(buf) - 1, 0);
    if (received <= 0) {
        closesocket(fd);
        return;
    }
    buf[received] = '\0';

    /* 解析请求行: METHOD /path HTTP/1.1 */
    char method[16], path[256];
    if (sscanf(buf, "%15s %255s", method, path) < 2) {
        send_error_json(fd, 400, "Bad Request");
        closesocket(fd);
        return;
    }

    /* 路由匹配 */
    if (strcmp(method, "GET") == 0) {
        if (strcmp(path, "/health") == 0) {
            handle_health(fd);
        } else if (strcmp(path, "/api/local/status") == 0) {
            handle_local_status(fd);
        } else {
            handle_not_found(fd);
        }
    } else {
        send_error_json(fd, 405, "Method Not Allowed");
    }

    closesocket(fd);
}

/* ============================================================
 * 服务线程
 * ============================================================ */

static DWORD WINAPI server_thread(LPVOID arg)
{
    (void)arg;

    while (g_local_server.running) {
        struct sockaddr_in client_addr;
        int addr_len = sizeof(client_addr);
        SOCKET client_fd = accept(g_local_server.listen_fd,
                                   (struct sockaddr*)&client_addr, &addr_len);

        if (client_fd == INVALID_SOCKET) {
            if (g_local_server.running) {
                LOG_DEBUG("本地HTTP服务 accept 失败: %d", WSAGetLastError());
            }
            break;
        }

        handle_client(client_fd);
    }

    return 0;
}

/* ============================================================
 * 公开 API
 * ============================================================ */

int client_http_server_start(uint16_t port)
{
    if (g_local_server.running) {
        LOG_WARN("本地HTTP服务已在运行中 (端口 %d)", g_local_server.port);
        return 0;
    }

    /* 创建 socket */
    g_local_server.listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_local_server.listen_fd == INVALID_SOCKET) {
        LOG_ERROR("创建本地HTTP服务 socket 失败: %d", WSAGetLastError());
        return -1;
    }

    /* 允许端口重用 */
    int optval = 1;
    setsockopt(g_local_server.listen_fd, SOL_SOCKET, SO_REUSEADDR,
               (const char*)&optval, sizeof(optval));

    /* 绑定 127.0.0.1:port */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(port);

    if (bind(g_local_server.listen_fd, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        LOG_ERROR("绑定本地HTTP服务端口 %d 失败: %d", port, WSAGetLastError());
        closesocket(g_local_server.listen_fd);
        g_local_server.listen_fd = INVALID_SOCKET;
        return -1;
    }

    /* 监听 */
    if (listen(g_local_server.listen_fd, LISTEN_BACKLOG) != 0) {
        LOG_ERROR("本地HTTP服务 listen 失败: %d", WSAGetLastError());
        closesocket(g_local_server.listen_fd);
        g_local_server.listen_fd = INVALID_SOCKET;
        return -1;
    }

    g_local_server.port = port;
    g_local_server.running = true;

    /* 启动服务线程 */
    g_local_server.thread = CreateThread(NULL, 0, server_thread, NULL, 0, NULL);
    if (!g_local_server.thread) {
        LOG_ERROR("创建本地HTTP服务线程失败: %d", GetLastError());
        g_local_server.running = false;
        closesocket(g_local_server.listen_fd);
        g_local_server.listen_fd = INVALID_SOCKET;
        return -1;
    }

    LOG_INFO("客户端本地HTTP服务已启动: 127.0.0.1:%d", port);
    return 0;
}

void client_http_server_stop(void)
{
    if (!g_local_server.running) return;

    g_local_server.running = false;

    /* 关闭监听 socket 以中断 accept */
    if (g_local_server.listen_fd != INVALID_SOCKET) {
        closesocket(g_local_server.listen_fd);
        g_local_server.listen_fd = INVALID_SOCKET;
    }

    /* 等待线程结束 */
    if (g_local_server.thread) {
        WaitForSingleObject(g_local_server.thread, 1000);
        CloseHandle(g_local_server.thread);
        g_local_server.thread = NULL;
    }

    LOG_INFO("客户端本地HTTP服务已停止");
}

bool client_http_server_is_running(void)
{
    return g_local_server.running;
}

uint16_t client_http_server_get_port(void)
{
    return g_local_server.port;
}
