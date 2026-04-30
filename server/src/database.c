/**
 * Chrono-shift 数据库操作 — 统一入口
 * 语言标准: C99
 *
 * 此文件已拆分为以下子模块：
 *   - db_core.c      : 路径构建、文件 I/O、init/close
 *   - db_users.c     : 用户 CRUD
 *   - db_messages.c  : 消息 CRUD
 *   - db_friends.c   : 好友 CRUD
 *   - db_templates.c : 模板 CRUD
 *
 * public API 声明保持于 database.h 不变。
 */
#include "database.h"
