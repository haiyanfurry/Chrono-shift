/**
 * Chrono-shift 跨平台兼容层 — 通用实现
 * 包含: 路径处理、时间/休眠函数
 */
#include "platform_compat.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ============================================================
 * 路径处理
 * ============================================================ */

void path_normalize(char* path)
{
    if (!path) return;
#ifdef PLATFORM_WINDOWS
    for (char* p = path; *p; p++) {
        if (*p == '\\') *p = '/';
    }
#else
    (void)path;
#endif
}

void path_join(char* dst, size_t dst_size, const char* base, const char* file)
{
    if (!dst || dst_size == 0) return;
    size_t base_len = base ? strlen(base) : 0;
    size_t file_len = file ? strlen(file) : 0;

    if (base_len + 1 + file_len + 1 > dst_size) {
        if (dst_size > 0) dst[0] = '\0';
        return;
    }

    if (base) memcpy(dst, base, base_len);
    dst[base_len] = '/';
    if (file) memcpy(dst + base_len + 1, file, file_len);
    dst[base_len + 1 + file_len] = '\0';
}

/* ============================================================
 * 时间/休眠
 * ============================================================ */

uint64_t now_ms(void)
{
#ifdef PLATFORM_WINDOWS
    return (uint64_t)GetTickCount64();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
#endif
}

void msleep(uint32_t ms)
{
#ifdef PLATFORM_WINDOWS
    Sleep(ms);
#else
    usleep((useconds_t)ms * 1000);
#endif
}

void usleep_wrap(uint32_t us)
{
#ifdef PLATFORM_WINDOWS
    Sleep(us / 1000 + 1);
#else
    usleep((useconds_t)us);
#endif
}
