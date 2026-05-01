#ifndef CHRONO_CLIENT_HTTP_SERVER_H
#define CHRONO_CLIENT_HTTP_SERVER_H

/**
 * Chrono-shift 客户端本地 HTTP 服务
 * 语言标准: C99
 *
 * 在客户端本地监听一个端口 (默认 9010)，为 WebView2 前端提供:
 * 1. 本地静态资源配置
 * 2. 本地 API 桥接（离线缓存、本地存储）
 * 3. 健康检查端点
 *
 * 注意: 此服务仅监听 127.0.0.1，不对外暴露。
 */

#include <stdint.h>
#include <stdbool.h>

/* ============================================================
 * 初始化与清理
 * ============================================================ */

/**
 * 启动客户端本地 HTTP 服务
 * @param port 监听端口 (默认 9010)
 * @return 0=成功, -1=失败
 */
int client_http_server_start(uint16_t port);

/**
 * 停止客户端本地 HTTP 服务
 */
void client_http_server_stop(void);

/**
 * 检查本地 HTTP 服务是否正在运行
 */
bool client_http_server_is_running(void);

/**
 * 获取本地 HTTP 服务监听端口
 */
uint16_t client_http_server_get_port(void);

#endif /* CHRONO_CLIENT_HTTP_SERVER_H */
