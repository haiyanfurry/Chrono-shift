#ifndef CHRONO_DATABASE_H
#define CHRONO_DATABASE_H

#include <stdint.h>
#include <stdbool.h>

/* ============================================================
 * 数据库操作 (SQLite3)
 * ============================================================ */

/* --- 初始化 --- */
int  db_init(const char* db_path);
void db_close(void);

/* --- 用户表操作 --- */
int  db_create_user(const char* username, const char* password_hash, 
                    const char* nickname, const char* avatar_url);
int  db_get_user_by_id(int64_t user_id, char** username, char** nickname, char** avatar_url);
int  db_get_user_by_username(const char* username, int64_t* user_id, 
                             char** password_hash, char** nickname);
int  db_update_user_profile(int64_t user_id, const char* nickname, const char* avatar_url);
int  db_search_users(const char* keyword, int64_t* results, size_t max_results, size_t* count);

/* --- 消息表操作 --- */
int  db_save_message(int64_t from_id, int64_t to_id, const char* content_encrypted,
                     int64_t* message_id);
int  db_get_messages(int64_t user1_id, int64_t user2_id, int64_t offset, int64_t limit,
                     int64_t* ids, int64_t* from_ids, char** contents, int64_t* timestamps,
                     size_t* count);
int  db_mark_message_read(int64_t message_id);

/* --- 好友表操作 --- */
int  db_add_friend(int64_t user_id, int64_t friend_id);
int  db_remove_friend(int64_t user_id, int64_t friend_id);
int  db_get_friends(int64_t user_id, int64_t* friend_ids, size_t max_count, size_t* count);
int  db_check_friendship(int64_t user_id, int64_t friend_id, bool* are_friends);

/* --- 模板表操作 --- */
int  db_create_template(const char* name, int64_t author_id, const char* css_path,
                        const char* preview_url, int64_t* template_id);
int  db_get_templates(int64_t offset, int64_t limit, int64_t* ids, char** names,
                      int64_t* author_ids, char** preview_urls, int64_t* downloads, size_t* count);
int  db_apply_template(int64_t user_id, int64_t template_id);
int  db_get_user_template(int64_t user_id, int64_t* template_id);
int  db_increment_template_downloads(int64_t template_id);

#endif /* CHRONO_DATABASE_H */
