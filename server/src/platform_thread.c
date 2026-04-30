/**
 * Chrono-shift 跨平台兼容层 — 线程与互斥体
 * 包含: thread_create / thread_join / mutex_init / mutex_lock / mutex_unlock / mutex_destroy
 */
#include "platform_compat.h"

#ifdef PLATFORM_WINDOWS

/* Windows CreateThread 需要 LPTHREAD_START_ROUTINE 签名 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"

int thread_create(thread_t* thread, void* (*func)(void*), void* arg)
{
    if (!thread) return -1;
    DWORD tid;
    *thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)func, arg, 0, &tid);
    return *thread ? 0 : -1;
}

#pragma GCC diagnostic pop

int thread_join(thread_t thread)
{
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
    return 0;
}

void mutex_init(mutex_t* mutex)
{
    if (mutex) InitializeCriticalSection(mutex);
}

void mutex_lock(mutex_t* mutex)
{
    if (mutex) EnterCriticalSection(mutex);
}

void mutex_unlock(mutex_t* mutex)
{
    if (mutex) LeaveCriticalSection(mutex);
}

void mutex_destroy(mutex_t* mutex)
{
    if (mutex) DeleteCriticalSection(mutex);
}

#else /* PLATFORM_LINUX */

#include <pthread.h>

int thread_create(thread_t* thread, void* (*func)(void*), void* arg)
{
    if (!thread) return -1;
    return pthread_create(thread, NULL, func, arg);
}

int thread_join(thread_t thread)
{
    return pthread_join(thread, NULL);
}

void mutex_init(mutex_t* mutex)
{
    if (mutex) pthread_mutex_init(mutex, NULL);
}

void mutex_lock(mutex_t* mutex)
{
    if (mutex) pthread_mutex_lock(mutex);
}

void mutex_unlock(mutex_t* mutex)
{
    if (mutex) pthread_mutex_unlock(mutex);
}

void mutex_destroy(mutex_t* mutex)
{
    if (mutex) pthread_mutex_destroy(mutex);
}

#endif
