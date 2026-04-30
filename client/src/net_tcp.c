/**
 * Chrono-shift 客户端网络层 - TCP/TLS 连接管理
 * 语言标准: C99
 *
 * Winsock 2.2 初始化/清理、TCP/TLS 连接、自动重连 (指数退避)
 * 提供 tcp_send_all / tcp_recv_all 供 HTTP 和 WebSocket 子模块使用
 */

#include "net_core.h"
#include "client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* TLS 抽象层 (同项目 server 模块) */
#include "../../server/include/tls_server.h"

/* ============================================================
 * Winsock 引用计数
 * ============================================================ */

static int g_winsock_count = 0;

/* ============================================================
 * Winsock 初始化/清理
 * ============================================================ */

int net_init(NetworkContext* ctx)
{
    memset(ctx, 0, sizeof(NetworkContext));
    ctx->sock = -1;
    ctx->max_retries = -1;      /* 默认无限重试 */
    ctx->auto_reconnect = true; /* 默认启用自动重连 */

    if (g_winsock_count == 0) {
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
            LOG_ERROR("Winsock 初始化失败");
            return -1;
        }
    }
    g_winsock_count++;

    LOG_INFO("网络模块初始化完成");
    return 0;
}

void net_cleanup(void)
{
    if (g_winsock_count > 0) {
        g_winsock_count--;
        if (g_winsock_count == 0) {
            WSACleanup();
        }
    }
}

/* ============================================================
 * TCP/TLS 连接管理
 * ============================================================ */

int net_connect(NetworkContext* ctx, const char* host, uint16_t port)
{
    if (!ctx || !host) return -1;

    /* 如果已有连接，先断开 */
    if (ctx->connected) {
        net_disconnect(ctx);
    }

    /* 重置重连计数 */
    ctx->reconnect_count = 0;

    if (ctx->use_tls) {
        /* === TLS 模式: 直接使用 tls_client_connect (避免双重连接) === */
        SSL* ssl = NULL;
        int fd = tls_client_connect(&ssl, host, port);
        if (fd < 0 || !ssl) {
            LOG_ERROR("TLS 连接失败: %s:%d", host, port);
            ctx->sock = -1;
            ctx->ssl  = NULL;
            ctx->connected = false;
            return -1;
        }
        ctx->sock = fd;
        ctx->ssl  = (void*)ssl;
    } else {
        /* === HTTP 明文模式 === */
        ctx->sock = (int)socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (ctx->sock < 0) {
            LOG_ERROR("socket() 失败: %d", WSAGetLastError());
            return -1;
        }

        /* 设置超时 */
        int timeout = 10000; /* 10秒 */
        setsockopt(ctx->sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
        setsockopt(ctx->sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));

        /* 解析服务器地址 */
        struct hostent* he = gethostbyname(host);
        if (!he) {
            LOG_ERROR("gethostbyname() 失败: %s", host);
            closesocket(ctx->sock);
            ctx->sock = -1;
            return -1;
        }

        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);

        if (connect(ctx->sock, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
            LOG_ERROR("connect() 失败: %d", WSAGetLastError());
            closesocket(ctx->sock);
            ctx->sock = -1;
            return -1;
        }
    }

    strncpy(ctx->server_host, host, sizeof(ctx->server_host) - 1);
    ctx->server_port = port;
    ctx->connected = true;

    LOG_INFO("已连接到服务器: %s:%d%s", host, port,
             ctx->use_tls ? " (HTTPS)" : "");

    return 0;
}

int net_set_tls(NetworkContext* ctx, bool enable)
{
    if (!ctx) return -1;
    ctx->use_tls = enable;
    ctx->ssl     = NULL;
    LOG_INFO("TLS %s", enable ? "已启用" : "已禁用");
    return 0;
}

/**
 * 自动重连 (指数退避)
 * 每次重连失败后等待时间翻倍，上限 30 秒
 */
int net_reconnect(NetworkContext* ctx)
{
    if (!ctx) return -1;

    /* 检查重试上限 */
    if (ctx->max_retries >= 0 && ctx->reconnect_count >= ctx->max_retries) {
        LOG_ERROR("重连已达上限 (%d 次)", ctx->max_retries);
        return -1;
    }

    /* 计算退避延迟: initial * factor^count, 上限 MAX_RECONNECT_DELAY_MS */
    int delay_ms = INITIAL_RECONNECT_DELAY_MS;
    for (int i = 0; i < ctx->reconnect_count; i++) {
        delay_ms *= RECONNECT_BACKOFF_FACTOR;
        if (delay_ms >= MAX_RECONNECT_DELAY_MS) {
            delay_ms = MAX_RECONNECT_DELAY_MS;
            break;
        }
    }

    ctx->reconnect_count++;
    LOG_INFO("重连中 (%d/%s)... 等待 %dms",
             ctx->reconnect_count,
             (ctx->max_retries < 0) ? "无限" : "有限",
             delay_ms);

    Sleep((DWORD)delay_ms);

    /* 执行重连 */
    int result = net_connect(ctx, ctx->server_host, ctx->server_port);
    if (result == 0) {
        LOG_INFO("重连成功 (%d 次尝试后)", ctx->reconnect_count);
        /* 成功后重置计数 */
        ctx->reconnect_count = 0;
    }

    return result;
}

int net_set_auto_reconnect(NetworkContext* ctx, bool enable, int max_retries)
{
    if (!ctx) return -1;
    ctx->auto_reconnect = enable;
    ctx->max_retries = max_retries;
    LOG_INFO("自动重连 %s (最大重试: %s)",
             enable ? "已启用" : "已禁用",
             max_retries < 0 ? "无限" : "有限");
    return 0;
}

void net_disconnect(NetworkContext* ctx)
{
    if (ctx && ctx->connected) {
        if (ctx->ssl) {
            tls_close((SSL*)ctx->ssl);
            ctx->ssl = NULL;
        }
        closesocket(ctx->sock);
        ctx->sock = -1;
        ctx->connected = false;
        LOG_INFO("网络连接已断开");
    }
}

bool net_is_connected(NetworkContext* ctx)
{
    return ctx ? ctx->connected : false;
}

/* ============================================================
 * TCP 收发辅助 (供 HTTP / WebSocket 子模块调用)
 * ============================================================ */

int tcp_send_all(NetworkContext* ctx, const uint8_t* data, size_t length)
{
    if (ctx->ssl) {
        /* TLS 加密发送 */
        size_t sent = 0;
        while (sent < length) {
            int n = (int)tls_write((SSL*)ctx->ssl, data + sent, (int)(length - sent));
            if (n < 0) return -1;
            sent += (size_t)n;
        }
        return 0;
    }
    /* 原始 TCP 发送 */
    size_t sent = 0;
    while (sent < length) {
        int n = send(ctx->sock, (const char*)(data + sent), (int)(length - sent), 0);
        if (n <= 0) return -1;
        sent += (size_t)n;
    }
    return 0;
}

int tcp_recv_all(NetworkContext* ctx, uint8_t* buffer, size_t length)
{
    if (ctx->ssl) {
        /* TLS 解密接收 */
        size_t received = 0;
        while (received < length) {
            int n = (int)tls_read((SSL*)ctx->ssl, buffer + received, (int)(length - received));
            if (n < 0) return -1;
            if (n == 0) return -1; /* 连接关闭 */
            received += (size_t)n;
        }
        return 0;
    }
    /* 原始 TCP 接收 */
    size_t received = 0;
    while (received < length) {
        int n = recv(ctx->sock, (char*)(buffer + received), (int)(length - received), 0);
        if (n <= 0) return -1;
        received += (size_t)n;
    }
    return 0;
}
