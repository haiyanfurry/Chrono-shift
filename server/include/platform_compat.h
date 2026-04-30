#ifndef PLATFORM_COMPAT_H
#define PLATFORM_COMPAT_H

/**
 * Chrono-shift 跨平台兼容层
 * 语言标准: C99
 *
 * 统一 Windows 和 Linux 的：
 *   - 目录遍历 (dir_iter)
 *   - Socket 类型 (socket_t)
 *   - 线程/互斥体
 *   - 时间/休眠函数
 *   - 路径分隔符
 *   - 文件操作
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* strcasecmp 跨平台兼容 */
#ifdef PLATFORM_WINDOWS
    #ifndef strcasecmp
        #define strcasecmp _stricmp
    #endif
#else
    #include <strings.h>
#endif

/* ============================================================
 * 平台检测
 * ============================================================ */
#if defined(_WIN32) || defined(_WIN64)
    #define PLATFORM_WINDOWS 1
#elif defined(__linux__) || defined(__unix__) || defined(__posix__)
    #define PLATFORM_LINUX 1
#else
    #error "Unsupported platform — only Windows and Linux are supported"
#endif

/* ============================================================
 * 路径分隔符
 * ============================================================ */
#ifdef PLATFORM_WINDOWS
    #define PATH_SEPARATOR '\\'
    #define PATH_SEPARATOR_STR "\\"
#else
    #define PATH_SEPARATOR '/'
    #define PATH_SEPARATOR_STR "/"
#endif

/* 规范化路径分隔符: 将 Windows 反斜杠统一为正斜杠 */
static inline void path_normalize(char* path)
{
    if (!path) return;
#ifdef PLATFORM_WINDOWS
    for (char* p = path; *p; p++) {
        if (*p == '\\') *p = '/';
    }
#else
    (void)path; /* Linux 下已经是正斜杠 */
#endif
}

/* 拼接路径: base + "/" + file */
static inline void path_join(char* dst, size_t dst_size, const char* base, const char* file)
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
 * Socket 类型抽象
 * ============================================================ */
#ifdef PLATFORM_WINDOWS
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <winsock2.h>
    #include <ws2tcpip.h>
    typedef SOCKET socket_t;
    #define INVALID_SOCKET_VAL INVALID_SOCKET
    #define SOCKET_ERRNO()     WSAGetLastError()
    #define close_socket(fd)   closesocket(fd)
    #define SOCKET_POLL(fd, events, timeout) WSAPoll(&(struct pollfd){fd, events, 0}, 1, timeout)
#else
    #include <sys/socket.h>
    #include <sys/types.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <errno.h>
    typedef int socket_t;
    #define INVALID_SOCKET_VAL (-1)
    #define SOCKET_ERRNO()     errno
    #define close_socket(fd)   close(fd)
#endif

/* 设置非阻塞 */
static inline int set_nonblocking(socket_t fd)
{
#ifdef PLATFORM_WINDOWS
    u_long mode = 1;
    return ioctlsocket(fd, FIONBIO, &mode) == 0 ? 0 : -1;
#else
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#endif
}

/* ============================================================
 * 目录迭代抽象
 * ============================================================ */
#ifdef PLATFORM_WINDOWS

typedef struct {
    HANDLE hFind;
    WIN32_FIND_DATAA find_data;
    char search_path[1024];
    bool first_call;
    bool valid;
} DirIterator;

static inline int dir_open(DirIterator* it, const char* path)
{
    if (!it || !path) return -1;
    memset(it, 0, sizeof(DirIterator));
    snprintf(it->search_path, sizeof(it->search_path), "%s/*", path);
    it->first_call = true;
    it->valid = false;
    return 0;
}

static inline int dir_next(DirIterator* it, char* name, size_t max_len)
{
    if (!it || !name || max_len == 0) return -1;

    if (it->first_call) {
        it->first_call = false;
        it->hFind = FindFirstFileA(it->search_path, &it->find_data);
        if (it->hFind == INVALID_HANDLE_VALUE) {
            return -1; /* 没有文件 */
        }
        it->valid = true;
    } else {
        if (!it->valid) return -1;
        if (!FindNextFileA(it->hFind, &it->find_data)) {
            it->valid = false;
            return -1;
        }
    }

    /* 跳过 "." 和 ".." */
    if (strcmp(it->find_data.cFileName, ".") == 0 ||
        strcmp(it->find_data.cFileName, "..") == 0) {
        return dir_next(it, name, max_len);
    }

    strncpy(name, it->find_data.cFileName, max_len - 1);
    name[max_len - 1] = '\0';
    return 0;
}

static inline void dir_close(DirIterator* it)
{
    if (it && it->valid) {
        FindClose(it->hFind);
        it->valid = false;
    }
}

#else /* PLATFORM_LINUX */

#include <dirent.h>

typedef struct {
    DIR* dp;
} DirIterator;

static inline int dir_open(DirIterator* it, const char* path)
{
    if (!it || !path) return -1;
    it->dp = opendir(path);
    return it->dp ? 0 : -1;
}

static inline int dir_next(DirIterator* it, char* name, size_t max_len)
{
    if (!it || !it->dp || !name || max_len == 0) return -1;

    struct dirent* entry;
    while ((entry = readdir(it->dp)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        strncpy(name, entry->d_name, max_len - 1);
        name[max_len - 1] = '\0';
        return 0;
    }
    return -1;
}

static inline void dir_close(DirIterator* it)
{
    if (it && it->dp) {
        closedir(it->dp);
        it->dp = NULL;
    }
}

#endif

/* ============================================================
 * 线程抽象
 * ============================================================ */
#ifdef PLATFORM_WINDOWS

typedef HANDLE thread_t;
typedef CRITICAL_SECTION mutex_t;

static inline int thread_create(thread_t* thread, void* (*func)(void*), void* arg)
{
    (void)func; /* Windows 使用不同的签名 */
    DWORD tid;
    *thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)func, arg, 0, &tid);
    return *thread ? 0 : -1;
}

static inline int thread_join(thread_t thread)
{
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
    return 0;
}

static inline void mutex_init(mutex_t* mutex)
{
    InitializeCriticalSection(mutex);
}

static inline void mutex_lock(mutex_t* mutex)
{
    EnterCriticalSection(mutex);
}

static inline void mutex_unlock(mutex_t* mutex)
{
    LeaveCriticalSection(mutex);
}

static inline void mutex_destroy(mutex_t* mutex)
{
    DeleteCriticalSection(mutex);
}

#else /* PLATFORM_LINUX */

#include <pthread.h>

typedef pthread_t thread_t;
typedef pthread_mutex_t mutex_t;

static inline int thread_create(thread_t* thread, void* (*func)(void*), void* arg)
{
    return pthread_create(thread, NULL, func, arg);
}

static inline int thread_join(thread_t thread)
{
    return pthread_join(thread, NULL);
}

static inline void mutex_init(mutex_t* mutex)
{
    pthread_mutex_init(mutex, NULL);
}

static inline void mutex_lock(mutex_t* mutex)
{
    pthread_mutex_lock(mutex);
}

static inline void mutex_unlock(mutex_t* mutex)
{
    pthread_mutex_unlock(mutex);
}

static inline void mutex_destroy(mutex_t* mutex)
{
    pthread_mutex_destroy(mutex);
}

#endif

/* ============================================================
 * 时间/休眠抽象
 * ============================================================ */
#ifdef PLATFORM_WINDOWS
    #include <synchapi.h>
    static inline void msleep(uint32_t ms) { Sleep(ms); }
    static inline void usleep_wrap(uint32_t us) { Sleep(us / 1000 + 1); }
#else
    #include <unistd.h>
    static inline void msleep(uint32_t ms) { usleep((useconds_t)ms * 1000); }
    static inline void usleep_wrap(uint32_t us) { usleep((useconds_t)us); }
#endif

/* 获取毫秒时间戳 (用于超时计算) */
static inline uint64_t now_ms(void)
{
#ifdef PLATFORM_WINDOWS
    return (uint64_t)GetTickCount64();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
#endif
}

/* ============================================================
 * 文件操作
 * ============================================================ */
#ifdef PLATFORM_WINDOWS
    #include <direct.h>  /* _mkdir */
    #define mkdir_p(path) _mkdir(path)
#else
    #include <sys/stat.h>
    #define mkdir_p(path) mkdir(path, 0755)
#endif

/* ============================================================
 * 网络初始化 (仅 Windows 需要 WSAStartup)
 * ============================================================ */
static inline int net_init(void)
{
#ifdef PLATFORM_WINDOWS
    WSADATA wsa;
    return WSAStartup(MAKEWORD(2, 2), &wsa) == 0 ? 0 : -1;
#else
    return 0; /* Linux 无需初始化 */
#endif
}

static inline void net_cleanup(void)
{
#ifdef PLATFORM_WINDOWS
    WSACleanup();
#endif
}

#endif /* PLATFORM_COMPAT_H */
