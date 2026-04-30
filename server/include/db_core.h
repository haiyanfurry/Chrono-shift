#ifndef CHRONO_DB_CORE_H
#define CHRONO_DB_CORE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 数据库核心内部接口
 *
 * 这些函数供 db_users / db_messages / db_friends / db_templates
 * 子模块使用，不对外公开。
 * ============================================================ */

#define DB_DIR_MODE  0755
#define DB_MAX_PATH  1024

/* 全局数据库基路径 */
extern char g_db_base[DB_MAX_PATH];

/* --- 路径构建器 --- */
void get_user_path(int64_t user_id, char* path, size_t path_size);
void get_users_dir(char* path, size_t path_size);
void get_next_id_path(char* path, size_t path_size);
void get_message_path(int64_t message_id, char* path, size_t path_size);
void get_messages_dir(char* path, size_t path_size);
void get_friendship_path(int64_t user_id, char* path, size_t path_size);
void get_friendships_dir(char* path, size_t path_size);
void get_template_path(int64_t template_id, char* path, size_t path_size);
void get_templates_dir(char* path, size_t path_size);

/* --- 文件 I/O 辅助 --- */
int64_t    read_next_id(void);
int        write_next_id(int64_t id);
int64_t    allocate_id(void);
char*      read_file_content(const char* path);
int        file_exists(const char* path);
int        ensure_dir(const char* path);

#ifdef __cplusplus
}
#endif

#endif /* CHRONO_DB_CORE_H */
