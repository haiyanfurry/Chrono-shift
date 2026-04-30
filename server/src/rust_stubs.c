/**
 * Chrono-shift Rust FFI 桩函数
 * 平台: Windows 测试用
 *
 * 当 Rust security 模块不可用时，提供基础功能的纯 C 实现
 * 密码使用简单哈希 (SHA256-like)，JWT 使用 Base64 编码
 * 注意: 仅用于开发/测试，生产环境必须链接 Rust 模块
 */

#include "server.h"
#include "database.h"
#include "http_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ============================================================
 * 密码哈希 (桩实现 - 使用简单哈希代替 Argon2id)
 * ============================================================ */

/* 简单哈希函数 (仅用于测试) */
static unsigned long simple_hash(const char* str)
{
    unsigned long hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + (unsigned char)c;
    return hash;
}

char* rust_hash_password(const char* password)
{
    if (!password) return NULL;
    unsigned long h1 = simple_hash(password);
    unsigned long h2 = simple_hash(password + strlen(password) / 2);
    char* result = (char*)malloc(64);
    if (!result) return NULL;
    snprintf(result, 64, "$2y$12$%08lx%08lx", h1, h2);
    return result;
}

int rust_verify_password(const char* password, const char* hash)
{
    if (!password || !hash) return 0;
    char* expected = rust_hash_password(password);
    if (!expected) return 0;
    int match = (strcmp(expected, hash) == 0) ? 1 : 0;
    free(expected);
    return match;
}

/* ============================================================
 * JWT Token 生成/验证 (桩实现)
 * ============================================================ */

char* rust_generate_jwt(const char* user_id)
{
    if (!user_id) return NULL;
    /* 生成简单的 base64-like token */
    time_t now = time(NULL);
    char header[128];
    snprintf(header, sizeof(header), "{\"alg\":\"HS256\",\"typ\":\"JWT\"}");
    char payload[256];
    snprintf(payload, sizeof(payload), "{\"sub\":\"%s\",\"iat\":%ld,\"exp\":%ld}",
             user_id, (long)now, (long)(now + 86400)); /* 24小时过期 */

    /* Base64 编码 (简化版) */
    static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    size_t hlen = strlen(header);
    size_t plen = strlen(payload);
    char* result = (char*)malloc(hlen * 2 + plen * 2 + 64);
    if (!result) return NULL;

    /* 简易编码 (非标准 base64，仅用于测试) */
    char* p = result;
    for (size_t i = 0; i < hlen; i += 3) {
        unsigned char b1 = (unsigned char)header[i];
        unsigned char b2 = (i+1 < hlen) ? (unsigned char)header[i+1] : 0;
        unsigned char b3 = (i+2 < hlen) ? (unsigned char)header[i+2] : 0;
        *p++ = b64[b1 >> 2];
        *p++ = b64[((b1 & 0x03) << 4) | (b2 >> 4)];
        *p++ = b64[((b2 & 0x0F) << 2) | (b3 >> 6)];
        *p++ = b64[b3 & 0x3F];
    }
    *p++ = '.';
    for (size_t i = 0; i < plen; i += 3) {
        unsigned char b1 = (unsigned char)payload[i];
        unsigned char b2 = (i+1 < plen) ? (unsigned char)payload[i+1] : 0;
        unsigned char b3 = (i+2 < plen) ? (unsigned char)payload[i+2] : 0;
        *p++ = b64[b1 >> 2];
        *p++ = b64[((b1 & 0x03) << 4) | (b2 >> 4)];
        *p++ = b64[((b2 & 0x0F) << 2) | (b3 >> 6)];
        *p++ = b64[b3 & 0x3F];
    }
    *p++ = '.';
    *p++ = 'd'; *p++ = 'e'; *p++ = 'v'; /* 签名占位 */
    *p = '\0';

    return result;
}

void rust_free_string(char* s)
{
    free(s);
}

/* ============================================================
 * server_init 实现
 * ============================================================ */

int server_init(const ServerConfig* config)
{
    if (!config) return -1;

    LOG_INFO("初始化 HTTP 服务器 (端口: %d)...", config->port);
    if (http_server_init(config) != 0) {
        LOG_ERROR("HTTP 服务器初始化失败");
        return -1;
    }

    LOG_INFO("初始化数据库...");
    if (db_init(config->db_path) != 0) {
        LOG_ERROR("数据库初始化失败: %s", config->db_path);
        return -1;
    }

    LOG_INFO("服务器初始化完成");
    return 0;
}
