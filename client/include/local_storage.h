#ifndef CHRONO_LOCAL_STORAGE_H
#define CHRONO_LOCAL_STORAGE_H

#include <stdint.h>
#include <stdbool.h>

/* ============================================================
 * 本地存储管理
 * 配置文件、缓存、主题文件管理
 * ============================================================ */

typedef struct {
    char base_path[1024];       /* 应用数据根路径 */
    char config_path[1024];     /* 配置文件路径 */
    char cache_path[1024];      /* 缓存路径 */
    char themes_path[1024];     /* 主题文件路径 */
} StorageContext;

/* --- API --- */
int  storage_init(StorageContext* ctx, const char* app_data_path);
int  storage_save_config(StorageContext* ctx, const char* key, const char* value);
int  storage_load_config(StorageContext* ctx, const char* key, char* value, size_t value_size);
int  storage_save_file(StorageContext* ctx, const char* relative_path, 
                       const uint8_t* data, size_t length);
int  storage_load_file(StorageContext* ctx, const char* relative_path,
                       uint8_t** data, size_t* length);
int  storage_delete_file(StorageContext* ctx, const char* relative_path);
bool storage_file_exists(StorageContext* ctx, const char* relative_path);

#endif /* CHRONO_LOCAL_STORAGE_H */
