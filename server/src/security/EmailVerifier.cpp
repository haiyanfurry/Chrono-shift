/**
 * Chrono-shift C++ 邮箱验证码发送器实现
 * SMTP 协议实现
 * C++17 重构版 (P9.2)
 */
#include "EmailVerifier.h"
#include "../util/Logger.h"
#include <cstring>
#include <sstream>
#include <vector>
#include <random>
#include <chrono>

// 平台 socket
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#endif

namespace chrono {
namespace security {

// ============================================================
// Base64 编码
// ============================================================

static const char BASE64_TABLE[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string EmailVerifier::base64_encode(const std::string& input) {
    std::string output;
    output.reserve(((input.size() + 2) / 3) * 4);

    size_t i = 0;
    const unsigned char* data = reinterpret_cast<const unsigned char*>(input.data());
    size_t len = input.size();

    while (i < len) {
        unsigned char a = (i < len) ? data[i++] : 0;
        unsigned char b = (i < len) ? data[i++] : 0;
        unsigned char c = (i < len) ? data[i++] : 0;

        output += BASE64_TABLE[a >> 2];
        output += BASE64_TABLE[((a & 0x03) << 4) | (b >> 4)];
        output += (i - 1 < len) ? BASE64_TABLE[((b & 0x0F) << 2) | (c >> 6)] : '=';
        output += (i < len) ? BASE64_TABLE[c & 0x3F] : '=';
    }

    return output;
}

// ============================================================
// 工具函数: 读取 socket 直到收到指定前缀
// ============================================================

#ifdef _WIN32
using SOCKET_TYPE = SOCKET;
constexpr SOCKET_TYPE INVALID_SOCK = INVALID_SOCKET;
#else
using SOCKET_TYPE = int;
constexpr SOCKET_TYPE INVALID_SOCK = -1;
#endif

static bool recv_response(SOCKET_TYPE sock, std::string& response, int timeout_ms = 5000) {
    response.clear();
    char buf[4096];
    int bytes_read;

    auto start = std::chrono::steady_clock::now();
    while (true) {
        // 检查超时
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
        if (elapsed >= timeout_ms) {
            break;
        }

#ifdef _WIN32
        bytes_read = recv(sock, buf, sizeof(buf) - 1, 0);
#else
        bytes_read = (int)::read(sock, buf, sizeof(buf) - 1);
#endif
        if (bytes_read > 0) {
            buf[bytes_read] = '\0';
            response += buf;

            // SMTP 响应以 \r\n 结尾, 检查是否完成
            // 多行响应: 以 '-' 分隔, 最后一行以 ' ' 分隔
            if (response.size() >= 5) {
                std::string last_four = response.substr(response.size() - 5);
                // 检查类似 "250 OK\r\n" 或 "250 \r\n" 格式
                if (last_four.find("\r\n") != std::string::npos) {
                    // 检查最后一行是否以空格开头 (非续行)
                    size_t last_line_start = response.rfind("\r\n", response.size() - 2);
                    if (last_line_start == std::string::npos) last_line_start = 0;
                    else last_line_start += 2;

                    if (last_line_start + 3 < response.size()) {
                        char code_check = response[last_line_start + 3];
                        if (code_check == ' ') {
                            break; // 最后一行, 完成
                        }
                    }
                }
            }
        } else if (bytes_read == 0) {
            // 连接关闭
            break;
        } else {
#ifdef _WIN32
            if (WSAGetLastError() == WSAEWOULDBLOCK) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
#endif
            break;
        }
    }

    return !response.empty();
}

static bool send_smtp_command(SOCKET_TYPE sock, const std::string& cmd,
                               std::string& response) {
#ifdef _WIN32
    if (send(sock, cmd.c_str(), (int)cmd.size(), 0) < 0) {
        return false;
    }
#else
    if (::write(sock, cmd.c_str(), cmd.size()) < 0) {
        return false;
    }
#endif
    return recv_response(sock, response);
}

// ============================================================
// EmailVerifier 实现
// ============================================================

EmailVerifier::EmailVerifier(SmtpConfig config)
    : config_(std::move(config)) {
    if (config_.from_addr.empty()) {
        config_.from_addr = config_.username;
    }
}

bool EmailVerifier::send_code(const std::string& to_email,
                               const std::string& code) {
    int sock = -1;

    if (!connect_server(sock)) {
        LOG_ERROR("[EmailVerifier] Failed to connect to SMTP server %s:%d",
                  config_.host.c_str(), config_.port);
        return false;
    }

    std::string response;

    // 读取欢迎信息
    if (!recv_response(sock, response)) {
        LOG_ERROR("[EmailVerifier] No welcome from SMTP server");
#ifdef _WIN32
        closesocket(sock);
#else
        ::close(sock);
#endif
        return false;
    }
    LOG_DEBUG("[EmailVerifier] SMTP welcome: %s", response.c_str());

    // EHLO
    std::string ehlo_cmd = "EHLO chrono-shift\r\n";
    if (!send_smtp_command(sock, ehlo_cmd, response)) {
        LOG_ERROR("[EmailVerifier] EHLO failed");
#ifdef _WIN32
        closesocket(sock);
#else
        ::close(sock);
#endif
        return false;
    }
    LOG_DEBUG("[EmailVerifier] EHLO response: %s", response.c_str());

    // AUTH LOGIN
    std::string auth_cmd = "AUTH LOGIN\r\n";
    if (!send_smtp_command(sock, auth_cmd, response)) {
        LOG_ERROR("[EmailVerifier] AUTH LOGIN failed");
#ifdef _WIN32
        closesocket(sock);
#else
        ::close(sock);
#endif
        return false;
    }
    LOG_DEBUG("[EmailVerifier] AUTH LOGIN: %s", response.c_str());

    // 发送 Base64 编码的用户名
    std::string user_b64 = base64_encode(config_.username) + "\r\n";
    if (!send_smtp_command(sock, user_b64, response)) {
        LOG_ERROR("[EmailVerifier] AUTH username failed");
#ifdef _WIN32
        closesocket(sock);
#else
        ::close(sock);
#endif
        return false;
    }
    LOG_DEBUG("[EmailVerifier] AUTH user: %s", response.c_str());

    // 发送 Base64 编码的密码
    std::string pass_b64 = base64_encode(config_.password) + "\r\n";
    if (!send_smtp_command(sock, pass_b64, response)) {
        LOG_ERROR("[EmailVerifier] AUTH password failed");
#ifdef _WIN32
        closesocket(sock);
#else
        ::close(sock);
#endif
        return false;
    }
    LOG_DEBUG("[EmailVerifier] AUTH pass: %s", response.c_str());

    // 检查认证是否成功 (235)
    if (response.size() < 3 || response.substr(0, 3) != "235") {
        LOG_ERROR("[EmailVerifier] Authentication failed: %s", response.c_str());
#ifdef _WIN32
        closesocket(sock);
#else
        ::close(sock);
#endif
        return false;
    }

    // MAIL FROM
    std::string mail_from = "MAIL FROM:<" + config_.from_addr + ">\r\n";
    if (!send_smtp_command(sock, mail_from, response)) {
        LOG_ERROR("[EmailVerifier] MAIL FROM failed");
#ifdef _WIN32
        closesocket(sock);
#else
        ::close(sock);
#endif
        return false;
    }

    // RCPT TO
    std::string rcpt_to = "RCPT TO:<" + to_email + ">\r\n";
    if (!send_smtp_command(sock, rcpt_to, response)) {
        LOG_ERROR("[EmailVerifier] RCPT TO failed");
#ifdef _WIN32
        closesocket(sock);
#else
        ::close(sock);
#endif
        return false;
    }

    // DATA
    std::string data_cmd = "DATA\r\n";
    if (!send_smtp_command(sock, data_cmd, response)) {
        LOG_ERROR("[EmailVerifier] DATA command failed");
#ifdef _WIN32
        closesocket(sock);
#else
        ::close(sock);
#endif
        return false;
    }

    // 邮件内容
    std::string email_body = build_email_body(to_email, code);
    if (!send_smtp_command(sock, email_body, response)) {
        LOG_ERROR("[EmailVerifier] Failed to send email body");
#ifdef _WIN32
        closesocket(sock);
#else
        ::close(sock);
#endif
        return false;
    }

    // 检查发送结果
    bool success = (response.size() >= 3 && response.substr(0, 3) == "250");

    // QUIT
    std::string quit_cmd = "QUIT\r\n";
    std::string quit_resp;
    send_smtp_command(sock, quit_cmd, quit_resp);

#ifdef _WIN32
    closesocket(sock);
#else
    ::close(sock);
#endif

    if (success) {
        LOG_INFO("[EmailVerifier] Verification code sent to %s", to_email.c_str());
    } else {
        LOG_ERROR("[EmailVerifier] Failed to send email: %s", response.c_str());
    }

    return success;
}

bool EmailVerifier::connect_server(int& sock) {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        LOG_ERROR("[EmailVerifier] WSAStartup failed");
        return false;
    }
#endif

    struct addrinfo hints, *res = nullptr;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    std::string port_str = std::to_string(config_.port);
    int ret = getaddrinfo(config_.host.c_str(), port_str.c_str(), &hints, &res);
    if (ret != 0 || !res) {
        LOG_ERROR("[EmailVerifier] DNS resolution failed for %s", config_.host.c_str());
#ifdef _WIN32
        WSACleanup();
#endif
        return false;
    }

    sock = (int)socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0) {
        LOG_ERROR("[EmailVerifier] socket() failed");
        freeaddrinfo(res);
#ifdef _WIN32
        WSACleanup();
#endif
        return false;
    }

#ifdef _WIN32
    // 设置超时
    int timeout = 10000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));
#endif

    if (connect(sock, res->ai_addr, (int)res->ai_addrlen) < 0) {
        LOG_ERROR("[EmailVerifier] connect() to %s:%d failed",
                  config_.host.c_str(), config_.port);
#ifdef _WIN32
        closesocket(sock);
        WSACleanup();
#else
        ::close(sock);
#endif
        freeaddrinfo(res);
        return false;
    }

    freeaddrinfo(res);
    return true;
}

bool EmailVerifier::send_command(int sock, const std::string& command,
                                  std::string& response, bool wait_response) {
#ifdef _WIN32
    if (send(sock, command.c_str(), (int)command.size(), 0) < 0) {
        LOG_ERROR("[EmailVerifier] send() failed");
        return false;
    }
#else
    if (::write(sock, command.c_str(), command.size()) < 0) {
        LOG_ERROR("[EmailVerifier] write() failed");
        return false;
    }
#endif

    if (wait_response) {
        return recv_response(sock, response);
    }
    return true;
}

std::string EmailVerifier::build_email_body(const std::string& to_email,
                                             const std::string& code) const {
    std::ostringstream body;
    body << "From: " << config_.from_name << " <" << config_.from_addr << ">\r\n"
         << "To: <" << to_email << ">\r\n"
         << "Subject: =?UTF-8?B?"
         << base64_encode("墨竹 - 邮箱验证码")
         << "?=\r\n"
         << "MIME-Version: 1.0\r\n"
         << "Content-Type: text/plain; charset=UTF-8\r\n"
         << "Content-Transfer-Encoding: base64\r\n"
         << "\r\n";

    // 邮件正文 (Base64)
    std::string text = "您好！\r\n\r\n"
                       "您的验证码是: " + code + "\r\n\r\n"
                       "验证码 5 分钟内有效，请勿泄露给他人。\r\n\r\n"
                       "—— 墨竹团队";

    body << base64_encode(text) << "\r\n"
         << ".\r\n";

    return body.str();
}

} // namespace security
} // namespace chrono
