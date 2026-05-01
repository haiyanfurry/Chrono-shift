/**
 * Chrono-shift 跨平台兼容层 — 网络初始化与 Socket 操作
 * 包含: net_init / net_cleanup / set_nonblocking / set_blocking
 */
#include "platform_compat.h"

#ifdef PLATFORM_WINDOWS

int net_init(void)
{
    WSADATA wsa;
    return WSAStartup(MAKEWORD(2, 2), &wsa) == 0 ? 0 : -1;
}

void net_cleanup(void)
{
    WSACleanup();
}

int set_nonblocking(socket_t fd)
{
    u_long mode = 1;
    return ioctlsocket(fd, FIONBIO, &mode) == 0 ? 0 : -1;
}

int set_blocking(socket_t fd)
{
    u_long mode = 0;
    return ioctlsocket(fd, FIONBIO, &mode) == 0 ? 0 : -1;
}

#else /* PLATFORM_LINUX */

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

int net_init(void)
{
    return 0;
}

void net_cleanup(void)
{
    /* Linux 无需清理 */
}

int set_nonblocking(socket_t fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int set_blocking(socket_t fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
}

#endif
