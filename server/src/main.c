/**
 * Chrono-shift 服务端入口
 * 语言标准: C99
 * 跨平台: Linux + Windows
 */

#include "platform_compat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>

#include "server.h"
#include "http_server.h"
#include "tls_server.h"
#include "websocket.h"
#include "database.h"
#include "user_handler.h"
#include "message_handler.h"
#include "community_handler.h"
#include "file_handler.h"

/* 全局服务器状态 */
static ServerConfig g_config;
static volatile bool g_running = false;

/* 默认配置 */
static void set_default_config(ServerConfig* config)
{
    memset(config, 0, sizeof(ServerConfig));
    strncpy(config->host, "0.0.0.0", sizeof(config->host) - 1);
    config->port = 8080;
    strncpy(config->db_path, "./data/db/chrono.db", sizeof(config->db_path) - 1);
    strncpy(config->storage_path, "./data/storage", sizeof(config->storage_path) - 1);
    strncpy(config->jwt_secret, "chrono-shift-jwt-secret-2024", sizeof(config->jwt_secret) - 1);
    config->thread_pool_size = 4;
    config->max_connections = 1024;
    config->log_level = LOG_INFO;
}

/* 信号处理 */
static void signal_handler(int signum)
{
    (void)signum;
    LOG_INFO("收到终止信号，正在关闭服务器...");
    g_running = false;
}

/* 注册 HTTP 路由 */
static void register_routes(void)
{
    /* 用户相关 */
    http_server_register_route("POST", "/api/user/register", 
                               handle_user_register, NULL);
    http_server_register_route("POST", "/api/user/login", 
                               handle_user_login, NULL);
    http_server_register_route("GET",  "/api/user/profile", 
                               handle_user_profile, NULL);
    http_server_register_route("PUT",  "/api/user/update", 
                               handle_user_update, NULL);
    http_server_register_route("GET",  "/api/user/search", 
                               handle_user_search, NULL);
    http_server_register_route("GET",  "/api/user/friends", 
                               handle_user_friends, NULL);
    http_server_register_route("POST", "/api/user/friends/add", 
                               handle_user_add_friend, NULL);

    /* 消息相关 */
    http_server_register_route("POST", "/api/message/send", 
                               handle_send_message, NULL);
    http_server_register_route("GET",  "/api/message/list", 
                               handle_get_messages, NULL);

    /* 社区模板相关 */
    http_server_register_route("GET",  "/api/templates", 
                               handle_template_list, NULL);
    http_server_register_route("POST", "/api/templates/upload", 
                               handle_template_upload, NULL);
    http_server_register_route("GET",  "/api/templates/download", 
                               handle_template_download, NULL);
    http_server_register_route("POST", "/api/templates/apply", 
                               handle_template_apply, NULL);

    /* 文件相关 */
    http_server_register_route("POST", "/api/file/upload", 
                               handle_file_upload, NULL);
    http_server_register_route("GET",  "/api/file/*", 
                               handle_static_file, NULL);
    http_server_register_route("POST", "/api/avatar/upload", 
                               handle_avatar_upload, NULL);
}

int main(int argc, char* argv[])
{
    /* 默认配置 */
    set_default_config(&g_config);

    /* 解析命令行参数 */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            g_config.port = (uint16_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--db") == 0 && i + 1 < argc) {
            strncpy(g_config.db_path, argv[++i], sizeof(g_config.db_path) - 1);
        } else if (strcmp(argv[i], "--storage") == 0 && i + 1 < argc) {
            strncpy(g_config.storage_path, argv[++i], sizeof(g_config.storage_path) - 1);
        } else if (strcmp(argv[i], "--log-level") == 0 && i + 1 < argc) {
            g_config.log_level = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--tls-cert") == 0 && i + 1 < argc) {
            strncpy(g_config.tls_cert, argv[++i], sizeof(g_config.tls_cert) - 1);
        } else if (strcmp(argv[i], "--tls-key") == 0 && i + 1 < argc) {
            strncpy(g_config.tls_key, argv[++i], sizeof(g_config.tls_key) - 1);
        } else if (strcmp(argv[i], "--help") == 0) {
            printf("Chrono-shift Server v0.1.0\n");
            printf("用法: chrono-server [选项]\n");
            printf("选项:\n");
            printf("  --port <port>          监听端口 (默认: 8080)\n");
            printf("  --db <path>            数据库路径 (默认: ./data/db/chrono.db)\n");
            printf("  --storage <path>       文件存储路径 (默认: ./data/storage)\n");
            printf("  --log-level <0-3>      日志级别 (默认: 1)\n");
            printf("  --tls-cert <path>      TLS 证书文件路径 (PEM 格式)\n");
            printf("  --tls-key <path>       TLS 私钥文件路径 (PEM 格式)\n");
            printf("  --help                 显示此帮助\n");
            return 0;
        }
    }

    /* 初始化各模块 */
    LOG_INFO("Chrono-shift 服务端启动中...");

    if (server_init(&g_config) != 0) {
        LOG_ERROR("服务器初始化失败");
        return 1;
    }

    /* 注册信号处理 */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /* 注册路由 */
    register_routes();

    /* 初始化 TLS（如果配置了证书） */
    if (g_config.tls_cert[0] != '\0' && g_config.tls_key[0] != '\0') {
        LOG_INFO("TLS 证书: %s", g_config.tls_cert);
        LOG_INFO("TLS 密钥: %s", g_config.tls_key);
        if (tls_server_init(g_config.tls_cert, g_config.tls_key) != 0) {
            LOG_ERROR("TLS 初始化失败");
            return 1;
        }
        LOG_INFO("TLS 已启用 (端口 %d 将同时支持 HTTP + HTTPS)", g_config.port);
    } else if (g_config.tls_cert[0] != '\0' || g_config.tls_key[0] != '\0') {
        LOG_ERROR("必须同时指定 --tls-cert 和 --tls-key");
        return 1;
    } else {
        LOG_INFO("TLS 未配置，仅 HTTP");
    }

    /* 启动服务器 */
    g_running = true;
    if (http_server_start() != 0) {
        LOG_ERROR("HTTP 服务器启动失败");
        return 1;
    }

    LOG_INFO("Chrono-shift 服务端已启动: %s:%d", g_config.host, g_config.port);
    LOG_INFO("按 Ctrl+C 停止服务器");

    /* 主循环 */
    while (g_running) {
        /* 每秒检查运行状态 */
        msleep(1000);
    }

    /* 清理 */
    LOG_INFO("正在关闭服务器...");
    http_server_stop();
    db_close();
    LOG_INFO("服务器已关闭");

    return 0;
}
