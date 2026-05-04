/**
 * TorClient.cpp — Tor SOCKS5 + Control Protocol 实现
 */
#include "tor/TorClient.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#define close_socket closesocket
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define close_socket close
#endif

#include <cstring>
#include <cstdio>

namespace chrono { namespace client { namespace tor {

// ============================================================
// 构造/析构
// ============================================================

TorClient::~TorClient() { disconnect_all(); }

void TorClient::disconnect_all()
{
    socks_close();
    control_disconnect();
}

// ============================================================
// SOCKS5 代理 (localhost:9050)
// ============================================================

static int tcp_connect(const std::string& host, uint16_t port)
{
    int fd = (int)socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
#ifdef _WIN32
    addr.sin_addr.s_addr = inet_addr(host.c_str());
#else
    inet_pton(AF_INET, host.c_str(), &addr.sin_addr);
#endif

    if (::connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close_socket(fd);
        return -1;
    }
    return fd;
}

bool TorClient::connect_socks(const std::string& host, uint16_t port)
{
    if (socks_fd_ >= 0) return true;

    socks_fd_ = tcp_connect(host, port);
    if (socks_fd_ < 0) {
        last_error_ = "无法连接 Tor SOCKS5 代理 (" + host + ":" + std::to_string(port) + ")";
        return false;
    }

    // SOCKS5 握手: 无认证
    uint8_t hello[] = {0x05, 0x01, 0x00};
    if (send(socks_fd_, (const char*)hello, 3, 0) != 3) {
        close_socket(socks_fd_); socks_fd_ = -1;
        return false;
    }

    uint8_t resp[2];
    if (recv(socks_fd_, (char*)resp, 2, 0) != 2) {
        close_socket(socks_fd_); socks_fd_ = -1;
        return false;
    }

    if (resp[0] != 0x05 || resp[1] != 0x00) {
        close_socket(socks_fd_); socks_fd_ = -1;
        last_error_ = "SOCKS5 握手失败";
        return false;
    }

    return true;
}

int TorClient::socks_connect(const std::string& onion_addr, uint16_t port)
{
    if (socks_fd_ < 0) return -1;

    // SOCKS5 CONNECT: ATYP=0x03 (domain), DST.ADDR=<onion>, DST.PORT=<port>
    size_t addr_len = onion_addr.size();
    if (addr_len > 255) return -1;

    size_t req_len = 5 + 1 + addr_len + 2;
    std::vector<uint8_t> req(req_len);
    req[0] = 0x05;               // VER
    req[1] = 0x01;               // CMD = CONNECT
    req[2] = 0x00;               // RSV
    req[3] = 0x03;               // ATYP = domain name
    req[4] = (uint8_t)addr_len;  // domain len
    memcpy(&req[5], onion_addr.c_str(), addr_len);
    req[5 + addr_len] = (port >> 8) & 0xFF;     // port hi
    req[5 + addr_len + 1] = port & 0xFF;         // port lo

    if (send(socks_fd_, (const char*)req.data(), (int)req_len, 0) != (int)req_len) {
        return -1;
    }

    // 读取 SOCKS5 响应
    uint8_t resp[262];  // max response
    int n = recv(socks_fd_, (char*)resp, sizeof(resp), 0);
    if (n < 10) return -1;

    if (resp[0] != 0x05 || resp[1] != 0x00) {
        last_error_ = "SOCKS5 CONNECT 失败: code=" + std::to_string(resp[1]);
        return -1;
    }

    return 0;  // 成功，可以在此 fd 上收发包
}

int TorClient::socks_send(const uint8_t* data, size_t len)
{
    if (socks_fd_ < 0) return -1;
    return (int)send(socks_fd_, (const char*)data, (int)len, 0);
}

int TorClient::socks_recv(uint8_t* buf, size_t max_len)
{
    if (socks_fd_ < 0) return -1;
    int n = recv(socks_fd_, (char*)buf, (int)max_len, 0);
    if (n <= 0) { close_socket(socks_fd_); socks_fd_ = -1; }
    return n;
}

void TorClient::socks_close()
{
    if (socks_fd_ >= 0) {
        close_socket(socks_fd_);
        socks_fd_ = -1;
    }
}

// ============================================================
// Control Protocol (localhost:9051)
// ============================================================

bool TorClient::connect_control(const std::string& host, uint16_t port,
                                 const std::string& password)
{
    if (ctrl_fd_ >= 0) return true;

    ctrl_fd_ = tcp_connect(host, port);
    if (ctrl_fd_ < 0) {
        last_error_ = "无法连接 Tor ControlPort";
        return false;
    }

    // 读取协议信息行
    char buf[1024];
    int n = recv(ctrl_fd_, buf, sizeof(buf) - 1, 0);
    if (n <= 0) { close_socket(ctrl_fd_); ctrl_fd_ = -1; return false; }
    buf[n] = '\0';

    // AUTHENTICATE
    std::string auth = "AUTHENTICATE";
    if (!password.empty()) {
        auth += " \"" + password + "\"";
    }
    auth += "\r\n";

    if (send(ctrl_fd_, auth.c_str(), (int)auth.size(), 0) < 0) {
        close_socket(ctrl_fd_); ctrl_fd_ = -1;
        return false;
    }

    n = recv(ctrl_fd_, buf, sizeof(buf) - 1, 0);
    if (n <= 0) { close_socket(ctrl_fd_); ctrl_fd_ = -1; return false; }
    buf[n] = '\0';

    if (!strstr(buf, "250")) {
        last_error_ = "Tor Control 认证失败: " + std::string(buf);
        close_socket(ctrl_fd_); ctrl_fd_ = -1;
        return false;
    }

    return true;
}

std::string TorClient::control_command(const std::string& cmd)
{
    if (ctrl_fd_ < 0) return "";

    std::string msg = cmd + "\r\n";
    if (send(ctrl_fd_, msg.c_str(), (int)msg.size(), 0) < 0) return "";

    char buf[4096];
    std::string result;
    while (true) {
        int n = recv(ctrl_fd_, buf, sizeof(buf) - 1, 0);
        if (n <= 0) break;
        buf[n] = '\0';
        result += buf;
        if (result.find("\r\n250 ") != std::string::npos ||
            result.find("\r\n551 ") != std::string::npos ||
            result.find("\r\n552 ") != std::string::npos) break;
        if (result.size() > 65536) break;
    }
    return result;
}

TorStatus TorClient::get_status()
{
    TorStatus s;
    std::string resp = control_command("GETINFO version nickname fingerprint"
                                        " traffic/read traffic/written"
                                        " status/circuit-established");
    s.connected = (ctrl_fd_ >= 0);

    auto extract = [&](const std::string& key) -> std::string {
        auto pos = resp.find(key + "=");
        if (pos == std::string::npos) return "";
        pos += key.size() + 1;
        auto end = resp.find('\n', pos);
        if (end == std::string::npos) end = resp.find("\r\n", pos);
        return resp.substr(pos, end - pos);
    };

    s.version = extract("version");
    s.nickname = extract("nickname");
    s.fingerprint = extract("fingerprint");
    s.circuits_active = 0; // 从 status/circuit-established 解析

    auto tr = extract("traffic/read");
    if (!tr.empty()) s.bytes_read = std::stoull(tr);
    auto tw = extract("traffic/written");
    if (!tw.empty()) s.bytes_written = std::stoull(tw);

    return s;
}

std::vector<TorCircuit> TorClient::get_circuits()
{
    std::vector<TorCircuit> circuits;
    std::string resp = control_command("GETINFO circuit-status");
    // 解析: <id> BUILT PURPOSE=<purpose> PATH=<path>
    size_t pos = 0;
    while (true) {
        auto end = resp.find('\n', pos);
        if (end == std::string::npos) break;
        std::string line = resp.substr(pos, end - pos);
        pos = end + 1;

        if (line.empty() || line[0] == '2' || line[0] == '.') continue;

        TorCircuit c;
        auto sp = line.find(' ');
        if (sp == std::string::npos) continue;
        c.id = line.substr(0, sp);

        auto sp2 = line.find(' ', sp + 1);
        c.status = line.substr(sp + 1, sp2 - sp - 1);

        // 提取 PURPOSE 和 PATH
        auto pp = line.find("PURPOSE=");
        if (pp != std::string::npos) {
            auto pe = line.find(' ', pp);
            c.purpose = line.substr(pp + 8, pe - pp - 8);
        }
        auto pathp = line.find("PATH=");
        if (pathp != std::string::npos) {
            auto pathe = line.find(' ', pathp);
            if (pathe == std::string::npos) pathe = line.size();
            c.path = line.substr(pathp + 5, pathe - pathp - 5);
        }
        circuits.push_back(c);
    }
    return circuits;
}

void TorClient::control_disconnect()
{
    if (ctrl_fd_ >= 0) {
        std::string quit = "QUIT\r\n";
        send(ctrl_fd_, quit.c_str(), (int)quit.size(), 0);
        close_socket(ctrl_fd_);
        ctrl_fd_ = -1;
    }
}

} } } // namespace
