#ifndef PLATFORM_COMPAT_H
#define PLATFORM_COMPAT_H

/**
 * Chrono-shift 跨平台兼容层
 * 语言标准: C99
 *
 * 注意: 函数实现在 server/src/platform_*.c 文件中
 * 此头文件仅包含宏定义、类型定义和函数原型
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

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
void path_normalize(char* path);

/* 拼接路径: base + "/" + file */
void path_join(char* dst, size_t dst_size, const char* base, const char* file);

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
int set_nonblocking(socket_t fd);

/* 设置阻塞 (清除非阻塞标志) */
int set_blocking(socket_t fd);

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

#else /* PLATFORM_LINUX */

#include <dirent.h>

typedef struct {
    DIR* dp;
} DirIterator;

#endif

int dir_open(DirIterator* it, const char* path);
int dir_next(DirIterator* it, char* name, size_t max_len);
void dir_close(DirIterator* it);

/* ============================================================
 * 线程抽象
 * ============================================================ */
#ifdef PLATFORM_WINDOWS

typedef HANDLE thread_t;
typedef CRITICAL_SECTION mutex_t;

#else /* PLATFORM_LINUX */

#include <pthread.h>

typedef pthread_t thread_t;
typedef pthread_mutex_t mutex_t;

#endif

int thread_create(thread_t* thread, void* (*func)(void*), void* arg);
int thread_join(thread_t thread);
void mutex_init(mutex_t* mutex);
void mutex_lock(mutex_t* mutex);
void mutex_unlock(mutex_t* mutex);
void mutex_destroy(mutex_t* mutex);

/* ============================================================
 * 时间/休眠抽象
 * ============================================================ */
#ifdef PLATFORM_WINDOWS
    #include <synchapi.h>
#else
    #include <unistd.h>
#endif

void msleep(uint32_t ms);
void usleep_wrap(uint32_t us);

/* 获取毫秒时间戳 (用于超时计算) */
uint64_t now_ms(void);

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
int net_init(void);
void net_cleanup(void);

#endif /* PLATFORM_COMPAT_H */
