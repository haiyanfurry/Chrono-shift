/**
 * Chrono-shift 本地存储管理 (骨架)
 * 语言标准: C99
 */

#include "local_storage.h"
#include "client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <direct.h>

int storage_init(StorageContext* ctx, const char* app_data_path)
{
    memset(ctx, 0, sizeof(StorageContext));
    strncpy(ctx->base_path, app_data_path, sizeof(ctx->base_path) - 1);

    /* 创建目录结构 */
    snprintf(ctx->config_path, sizeof(ctx->config_path), "%s/config", app_data_path);
    snprintf(ctx->cache_path, sizeof(ctx->cache_path), "%s/cache", app_data_path);
    snprintf(ctx->themes_path, sizeof(ctx->themes_path), "%s/themes", app_data_path);

    /* 创建必要目录 */
    _mkdir(ctx->config_path);
    _mkdir(ctx->cache_path);
    _mkdir(ctx->themes_path);

    LOG_INFO("本地存储初始化完成: %s", app_data_path);
    return 0;
}

int storage_save_config(StorageContext* ctx, const char* key, const char* value)
{
    (void)ctx;
    (void)key;
    (void)value;
    /* Phase 3 实现 */
    return 0;
}

int storage_load_config(StorageContext* ctx, const char* key, char* value, size_t value_size)
{
    (void)ctx;
    (void)key;
    (void)value;
    (void)value_size;
    /* Phase 3 实现 */
    return 0;
}

int storage_save_file(StorageContext* ctx, const char* relative_path,
                      const uint8_t* data, size_t length)
{
    (void)ctx;
    (void)relative_path;
    (void)data;
    (void)length;
    /* Phase 5 实现 */
    return 0;
}

int storage_load_file(StorageContext* ctx, const char* relative_path,
                      uint8_t** data, size_t* length)
{
    (void)ctx;
    (void)relative_path;
    (void)data;
    (void)length;
    /* Phase 5 实现 */
    return 0;
}

int storage_delete_file(StorageContext* ctx, const char* relative_path)
{
    (void)ctx;
    (void)relative_path;
    return 0;
}

bool storage_file_exists(StorageContext* ctx, const char* relative_path)
{
    (void)ctx;
    (void)relative_path;
    return false;
}
