/**
 * test_s_ctx_stub.c — 为 http_parse 测试提供最小 s_ctx 定义
 *
 * http_parse.c 中的 find_route() 通过 #ifndef UNIT_TESTING 排除，
 * 但 http_core.h 中 extern ServerContext s_ctx; 的声明仍需要链接符号。
 * 此桩文件仅提供符号定义，不包含任何依赖（网络、TLS、epoll 等）。
 */
#include "http_core.h"
#include "http_server.h"
#include <string.h>

/* 为测试提供全局 s_ctx 定义 */
ServerContext s_ctx;
bool s_initialized = false;
