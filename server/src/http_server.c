/**
 * Chrono-shift HTTP 服务器 (已拆分)
 *
 * 此文件已被拆分为以下子模块，请编辑对应的文件而非本文件：
 *
 * ┌─ server/include/http_core.h     内部头文件 (数据结构、跨模块函数声明)
 * ├─ server/src/http_core.c         服务器生命周期 (init/start/stop/register_route)
 * ├─ server/src/http_conn.c         连接管理 + I/O + 事件循环
 * ├─ server/src/http_parse.c        HTTP 解析 + 路由 + 公共辅助函数
 * └─ server/src/http_response.c     响应构建 API
 *
 * 保留此文件仅用于兼容性——功能实现已移至上述各文件。
 */
#include "http_server.h"
