/**
 * Chrono-shift 客户端网络层 - HTTP 请求/响应
 * 语言标准: C99
 *
 * HTTP/1.1 请求构建与响应解析，支持自动重连重试
 */

#include "net_core.h"
#include "client.h"
#include <stdio.h>

/* TLS 抽象层 */
#include "../../server/include/tls_server.h"
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * 常量
 * ============================================================ */

#define NET_BUF_SIZE      65536
#define HTTP_PROTO        "HTTP/1.1"

/* ============================================================
 * HTTP 请求/响应 (带自动重连)
 * ============================================================ */

int net_http_request(NetworkContext* ctx, const char* method, const char* path,
                     const char* headers, const uint8_t* body, size_t body_len,
                     char** response_body, size_t* response_len)
{
    if (!ctx || !method || !path) {
        return -1;
    }

    /* 如果未连接，尝试自动重连 */
    if (!ctx->connected) {
        if (ctx->auto_reconnect && ctx->server_host[0] != '\0') {
            if (net_reconnect(ctx) != 0) {
                return -1;
            }
        } else {
            return -1;
        }
    }

    /* 构建 HTTP 请求 */
    char request[NET_BUF_SIZE];
    int n;

    if (headers) {
        if (body && body_len > 0) {
            n = snprintf(request, sizeof(request),
                         "%s %s %s\r\n"
                         "Host: %s\r\n"
                         "Content-Length: %zu\r\n"
                         "%s\r\n"
                         "\r\n",
                         method, path, HTTP_PROTO,
                         ctx->server_host,
                         body_len,
                         headers);
        } else {
            n = snprintf(request, sizeof(request),
                         "%s %s %s\r\n"
                         "Host: %s\r\n"
                         "%s\r\n"
                         "\r\n",
                         method, path, HTTP_PROTO,
                         ctx->server_host,
                         headers);
        }
    } else {
        if (body && body_len > 0) {
            n = snprintf(request, sizeof(request),
                         "%s %s %s\r\n"
                         "Host: %s\r\n"
                         "Content-Length: %zu\r\n"
                         "\r\n",
                         method, path, HTTP_PROTO,
                         ctx->server_host,
                         body_len);
        } else {
            n = snprintf(request, sizeof(request),
                         "%s %s %s\r\n"
                         "Host: %s\r\n"
                         "\r\n",
                         method, path, HTTP_PROTO,
                         ctx->server_host);
        }
    }

    if (n < 0 || (size_t)n >= sizeof(request)) {
        LOG_ERROR("HTTP 请求头过长");
        return -1;
    }

    /* 发送请求头 + 请求体 (带重试) */
    for (int attempt = 0; attempt < 2; attempt++) {
        if (attempt > 0) {
            /* 第一次失败后，尝试重连再试一次 */
            if (ctx->auto_reconnect && ctx->server_host[0] != '\0') {
                if (net_reconnect(ctx) != 0) {
                    return -1;
                }
            } else {
                return -1;
            }
        }

        if (tcp_send_all(ctx, (uint8_t*)request, (size_t)n) != 0) {
            LOG_WARN("发送 HTTP 请求失败 (尝试 %d/2)", attempt + 1);
            ctx->connected = false;
            continue;
        }

        /* 发送请求体 */
        if (body && body_len > 0) {
            if (tcp_send_all(ctx, body, body_len) != 0) {
                LOG_WARN("发送 HTTP body 失败 (尝试 %d/2)", attempt + 1);
                ctx->connected = false;
                continue;
            }
        }

        /* --- 接收响应 --- */
        uint8_t buffer[NET_BUF_SIZE];
        size_t buf_pos = 0;
        int in_body = 0;
        size_t content_length = 0;
        size_t body_received = 0;
        char* body_start = NULL;
        int recv_ok = 1;

        while (buf_pos < sizeof(buffer) - 1) {
            int n_recv;
            if (ctx->ssl) {
                n_recv = (int)tls_read((SSL*)ctx->ssl, buffer + buf_pos,
                                       (int)(sizeof(buffer) - buf_pos - 1));
            } else {
                n_recv = recv(ctx->sock, (char*)(buffer + buf_pos),
                              (int)(sizeof(buffer) - buf_pos - 1), 0);
            }
            if (n_recv <= 0) {
                recv_ok = 0;
                break;
            }
            buf_pos += (size_t)n_recv;
            buffer[buf_pos] = '\0';

            if (!in_body) {
                /* 查找 \r\n\r\n 分隔符 */
                char* header_end = strstr((char*)buffer, "\r\n\r\n");
                if (header_end) {
                    in_body = 1;
                    body_start = header_end + 4;
                    body_received = buf_pos - (size_t)(body_start - (char*)buffer);

                    /* 解析 Content-Length */
                    char* cl = strstr((char*)buffer, "Content-Length:");
                    if (cl) {
                        cl += 16;
                        while (*cl == ' ') cl++;
                        content_length = (size_t)atol(cl);
                    } else {
                        /* 没有 Content-Length，读取所有数据 */
                        content_length = buf_pos;
                    }
                }
            }

            if (in_body) {
                if (content_length > 0 && body_received >= content_length) {
                    break;
                }
            }
        }

        if (!in_body) {
            LOG_WARN("HTTP 响应不完整 (尝试 %d/2)", attempt + 1);
            ctx->connected = false;
            continue;
        }

        if (!recv_ok && body_received == 0) {
            LOG_WARN("HTTP 接收失败 (尝试 %d/2)", attempt + 1);
            ctx->connected = false;
            continue;
        }

        /* 提取 body */
        size_t actual_body_len = body_received;
        if (content_length > 0 && body_received > content_length) {
            actual_body_len = content_length;
        } else if (content_length > 0) {
            actual_body_len = content_length;
        }

        if (response_body && response_len) {
            *response_body = (char*)malloc(actual_body_len + 1);
            if (*response_body) {
                memcpy(*response_body, body_start, actual_body_len);
                (*response_body)[actual_body_len] = '\0';
                *response_len = actual_body_len;
            } else {
                *response_len = 0;
            }
        }

        return 0;
    }

    return -1;
}
