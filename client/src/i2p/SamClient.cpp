/**
 * SamClient.cpp — I2P SAM v3 客户端实现
 *
 * 支持两种模式:
 * - local: 本地 TCP 模拟 (开发测试用)
 * - i2p: 真实 I2P SAM 桥接 (需要 I2P 路由器运行在 localhost:7656)
 */
#include "i2p/SamClient.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
#endif

#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <random>

namespace chrono { namespace client { namespace i2p {

static std::string generate_random_b32()
{
    static const char* alphabet = "abcdefghijklmnopqrstuvwxyz234567";
    std::string result(52, ' ');
    std::mt19937 rng(static_cast<unsigned>(std::time(nullptr)));
    for (size_t i = 0; i < 52; i++) {
        result[i] = alphabet[rng() % 32];
    }
    return result + ".b32.i2p";
}

SamClient::~SamClient()
{
    disconnect();
}

bool SamClient::connect(const std::string& host, uint16_t port)
{
    if (sam_fd_ >= 0) return true;

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

#ifdef _WIN32
    addr.sin_addr.s_addr = inet_addr(host.c_str());
#else
    inet_pton(AF_INET, host.c_str(), &addr.sin_addr);
#endif

    sam_fd_ = (int)socket(AF_INET, SOCK_STREAM, 0);
    if (sam_fd_ < 0) return false;

    if (::connect(sam_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        // I2P 路由器不可用, 回退到本地模拟模式
        local_mode_ = true;
        closesocket(sam_fd_);
        sam_fd_ = -1;
        return create_session("chrono");  // 直接创建模拟会话
    }

    return true;
}

bool SamClient::create_session(const std::string& session_id)
{
    session_.session_id = session_id;

    if (local_mode_) {
        session_.destination = generate_random_b32();
        session_.active = true;
        return true;
    }

    // SAM v3 握手: HELLO + SESSION CREATE
    char buf[4096];
    snprintf(buf, sizeof(buf), "HELLO VERSION MIN=3.0 MAX=3.1\n");
    if (::send(sam_fd_, buf, (int)strlen(buf), 0) < 0) return false;

    int n = recv(sam_fd_, buf, sizeof(buf) - 1, 0);
    if (n <= 0) return false;
    buf[n] = '\0';

    snprintf(buf, sizeof(buf),
        "SESSION CREATE STYLE=STREAM ID=%s DESTINATION=transient\n",
        session_id.c_str());
    if (::send(sam_fd_, buf, (int)strlen(buf), 0) < 0) return false;

    n = recv(sam_fd_, buf, sizeof(buf) - 1, 0);
    if (n <= 0) return false;
    buf[n] = '\0';

    // 解析 DESTINATION= 字段
    const char* dest = strstr(buf, "DESTINATION=");
    if (dest) {
        dest += 12;
        const char* end = dest;
        while (*end && *end != ' ' && *end != '\n') end++;
        session_.destination = std::string(dest, end - dest);
    } else {
        session_.destination = generate_random_b32();
    }

    session_.active = true;
    return true;
}

std::string SamClient::naming_lookup(const std::string& b32_addr)
{
    if (local_mode_) return b32_addr;  // 模拟: 直接返回

    char buf[1024];
    snprintf(buf, sizeof(buf), "NAMING LOOKUP NAME=%s\n", b32_addr.c_str());
    ::send(sam_fd_, buf, (int)strlen(buf), 0);

    int n = recv(sam_fd_, buf, sizeof(buf) - 1, 0);
    if (n <= 0) return b32_addr;
    buf[n] = '\0';
    return b32_addr;
}

bool SamClient::stream_connect(const std::string& destination)
{
    if (local_mode_) {
        // 本地模拟: 用 TCP localhost:9876 模拟 I2P 流
        stream_fd_ = (int)socket(AF_INET, SOCK_STREAM, 0);
        if (stream_fd_ < 0) return false;

        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(9876);
        addr.sin_addr.s_addr = inet_addr("127.0.0.1");

        return ::connect(stream_fd_, (struct sockaddr*)&addr, sizeof(addr)) >= 0;
    }

    char buf[1024];
    snprintf(buf, sizeof(buf), "STREAM CONNECT ID=%s DESTINATION=%s\n",
             session_.session_id.c_str(), destination.c_str());
    ::send(sam_fd_, buf, (int)strlen(buf), 0);

    int n = recv(sam_fd_, buf, sizeof(buf) - 1, 0);
    if (n <= 0) return false;
    buf[n] = '\0';
    return strstr(buf, "RESULT=OK") != nullptr;
}

int SamClient::stream_send(const uint8_t* data, size_t len)
{
    int fd = local_mode_ ? stream_fd_ : sam_fd_;
    if (fd < 0) return -1;
    return (int)::send(fd, (const char*)data, (int)len, 0);
}

int SamClient::stream_recv(uint8_t* buf, size_t max_len)
{
    int fd = local_mode_ ? stream_fd_ : sam_fd_;
    if (fd < 0) return -1;
    int n = recv(fd, (char*)buf, (int)max_len, 0);
    return n;
}

void SamClient::stream_close()
{
    if (stream_fd_ >= 0) {
        closesocket(stream_fd_);
        stream_fd_ = -1;
    }
}

void SamClient::disconnect()
{
    stream_close();
    if (sam_fd_ >= 0) {
        closesocket(sam_fd_);
        sam_fd_ = -1;
    }
    session_.active = false;
}

} } } // namespace
