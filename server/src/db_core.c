/**
 * Chrono-shift 数据库核心模块
 * 语言标准: C99
 *
 * 包含：内部常量、路径构建器、文件 I/O 辅助、db_init / db_close
 */

#include "database.h"
#include "db_core.h"
#include "server.h"
#include "json_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "platform_compat.h"
#include <sys/stat.h>

/* ============================================================
 * 内部常量
 * ============================================================ */

#define USER_DIR       "users"
#define MESSAGE_DIR    "messages"
#define FRIENDSHIP_DIR "friendships"
#define TEMPLATE_DIR   "templates"
#define NEXT_ID_FILE   "next_id.txt"

/* 全局数据库基路径 */
char g_db_base[DB_MAX_PATH] = {0};

/* ============================================================
 * 路径构建器
 * ============================================================ */

void get_user_path(int64_t user_id, char* path, size_t path_size)
{
    snprintf(path, path_size, "%s/%s/%lld.json",
             g_db_base, USER_DIR, (long long)user_id);
}

void get_users_dir(char* path, size_t path_size)
{
    snprintf(path, path_size, "%s/%s", g_db_base, USER_DIR);
}

void get_next_id_path(char* path, size_t path_size)
{
    snprintf(path, path_size, "%s/%s", g_db_base, NEXT_ID_FILE);
}

void get_message_path(int64_t message_id, char* path, size_t path_size)
{
    snprintf(path, path_size, "%s/%s/%lld.json",
             g_db_base, MESSAGE_DIR, (long long)message_id);
}

void get_messages_dir(char* path, size_t path_size)
{
    snprintf(path, path_size, "%s/%s", g_db_base, MESSAGE_DIR);
}

void get_friendship_path(int64_t user_id, char* path, size_t path_size)
{
    snprintf(path, path_size, "%s/%s/%lld.json",
             g_db_base, FRIENDSHIP_DIR, (long long)user_id);
}

void get_friendships_dir(char* path, size_t path_size)
{
    snprintf(path, path_size, "%s/%s", g_db_base, FRIENDSHIP_DIR);
}

void get_template_path(int64_t template_id, char* path, size_t path_size)
{
    snprintf(path, path_size, "%s/%s/%lld.json",
             g_db_base, TEMPLATE_DIR, (long long)template_id);
}

void get_templates_dir(char* path, size_t path_size)
{
    snprintf(path, path_size, "%s/%s", g_db_base, TEMPLATE_DIR);
}

/* ============================================================
 * 文件 I/O 辅助
 * ============================================================ */

int64_t read_next_id(void)
{
    char path[DB_MAX_PATH];
    get_next_id_path(path, sizeof(path));

    FILE* f = fopen(path, "r");
    if (!f) return 1; /* 从 1 开始 */

    int64_t id = 0;
    if (fscanf(f, "%lld", (long long*)&id) != 1) id = 1;
    fclose(f);
    return id;
}

int write_next_id(int64_t id)
{
    char path[DB_MAX_PATH];
    get_next_id_path(path, sizeof(path));

    FILE* f = fopen(path, "w");
    if (!f) return -1;

    fprintf(f, "%lld", (long long)id);
    fclose(f);
    return 0;
}

int64_t allocate_id(void)
{
    int64_t id = read_next_id();
    if (write_next_id(id + 1) != 0) return -1;
    return id;
}

char* read_file_content(const char* path)
{
    FILE* f = fopen(path, "r");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0) {
        fclose(f);
        return NULL;
    }

    char* content = (char*)malloc((size_t)size + 1);
    if (!content) {
        fclose(f);
        return NULL;
    }

    size_t n = fread(content, 1, (size_t)size, f);
    content[n] = '\0';
    fclose(f);
    return content;
}

int file_exists(const char* path)
{
    FILE* f = fopen(path, "r");
    if (f) {
        fclose(f);
        return 1;
    }
    return 0;
}

int ensure_dir(const char* path)
{
    struct stat st = {0};
    if (stat(path, &st) == -1) {
#ifdef PLATFORM_WINDOWS
        return _mkdir(path);
#else
        return mkdir(path, DB_DIR_MODE);
#endif
    }
    return 0;
}

/* ============================================================
 * API 实现：初始化 / 关闭
 * ============================================================ */

int db_init(const char* db_path)
{
    LOG_INFO("初始化数据库: %s", db_path);

    strncpy(g_db_base, db_path, sizeof(g_db_base) - 1);

    /* 创建目录结构 */
    if (ensure_dir(g_db_base) != 0) {
        LOG_ERROR("创建数据库目录失败: %s", g_db_base);
        return -1;
    }

    char users_dir[DB_MAX_PATH];
    get_users_dir(users_dir, sizeof(users_dir));
    if (ensure_dir(users_dir) != 0) {
        LOG_ERROR("创建用户目录失败: %s", users_dir);
        return -1;
    }

    /* 确保 messages 目录存在 */
    char msg_dir[DB_MAX_PATH];
    get_messages_dir(msg_dir, sizeof(msg_dir));
    if (ensure_dir(msg_dir) != 0) {
        LOG_ERROR("创建消息目录失败: %s", msg_dir);
        return -1;
    }

    /* 确保 friendships 目录存在 */
    char fr_dir[DB_MAX_PATH];
    get_friendships_dir(fr_dir, sizeof(fr_dir));
    if (ensure_dir(fr_dir) != 0) {
        LOG_ERROR("创建好友目录失败: %s", fr_dir);
        return -1;
    }

    /* 确保 templates 目录存在 */
    char tmpl_dir[DB_MAX_PATH];
    get_templates_dir(tmpl_dir, sizeof(tmpl_dir));
    if (ensure_dir(tmpl_dir) != 0) {
        LOG_ERROR("创建模板目录失败: %s", tmpl_dir);
        return -1;
    }

    /* 确保 next_id 文件存在 */
    char id_path[DB_MAX_PATH];
    get_next_id_path(id_path, sizeof(id_path));
    if (!file_exists(id_path)) {
        write_next_id(1);
    }

    LOG_INFO("数据库初始化完成: %s", g_db_base);
    return 0;
}

void db_close(void)
{
    LOG_INFO("数据库已关闭");
}
