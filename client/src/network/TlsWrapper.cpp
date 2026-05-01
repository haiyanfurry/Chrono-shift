/**
 * Chrono-shift 客户端 TLS RAII 包装实现
 * C++17 重构版
 */
#include "TlsWrapper.h"

#include <cstring>
#include <stdexcept>

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "tls_client.h"

namespace chrono {
namespace client {
namespace network {

// ============================================================
// TlsConnection
// ============================================================

TlsConnection::TlsConnection(int fd, struct ssl_st* ssl) noexcept
    : fd_(fd)
    , ssl_(ssl)
{
}

TlsConnection::~TlsConnection()
{
    cleanup();
}

TlsConnection::TlsConnection(TlsConnection&& other) noexcept
    : fd_(other.fd_)
    , ssl_(other.ssl_)
{
    other.fd_ = -1;
    other.ssl_ = nullptr;
}

TlsConnection& TlsConnection::operator=(TlsConnection&& other) noexcept
{
    if (this != &other) {
        cleanup();
        fd_ = other.fd_;
        ssl_ = other.ssl_;
        other.fd_ = -1;
        other.ssl_ = nullptr;
    }
    return *this;
}

int TlsConnection::read(void* buf, int len) noexcept
{
    if (!ssl_) return -1;
    return static_cast<int>(tls_read(ssl_, buf, len));
}

int TlsConnection::write(const void* buf, int len) noexcept
{
    if (!ssl_) return -1;
    return static_cast<int>(tls_write(ssl_, buf, len));
}

void TlsConnection::cleanup() noexcept
{
    if (ssl_) {
        tls_close(ssl_);
        ssl_ = nullptr;
    }
    if (fd_ >= 0) {
#ifdef _WIN32
        closesocket(fd_);
#else
        ::close(fd_);
#endif
        fd_ = -1;
    }
}

// ============================================================
// TlsClientContext
// ============================================================

TlsClientContext::TlsClientContext(const std::string& ca_file)
    : initialized_(false)
{
    const char* ca_path = ca_file.empty() ? nullptr : ca_file.c_str();
    if (tls_client_init(ca_path) != 0) {
        throw std::runtime_error("TLS 客户端初始化失败");
    }
    initialized_ = true;
}

TlsClientContext::~TlsClientContext()
{
    if (initialized_) {
        tls_client_cleanup();
        initialized_ = false;
    }
}

TlsClientContext::TlsClientContext(TlsClientContext&& other) noexcept
    : initialized_(other.initialized_)
{
    other.initialized_ = false;
}

TlsClientContext& TlsClientContext::operator=(TlsClientContext&& other) noexcept
{
    if (this != &other) {
        if (initialized_) {
            tls_client_cleanup();
        }
        initialized_ = other.initialized_;
        other.initialized_ = false;
    }
    return *this;
}

std::unique_ptr<TlsConnection> TlsClientContext::connect(
    const std::string& host, uint16_t port)
{
    if (!initialized_) {
        return nullptr;
    }

    SSL* ssl = nullptr;
    int fd = tls_client_connect(&ssl, host.c_str(), port);
    if (fd < 0 || !ssl) {
        return nullptr;
    }

    return std::make_unique<TlsConnection>(fd, ssl);
}

} // namespace network
} // namespace client
} // namespace chrono
