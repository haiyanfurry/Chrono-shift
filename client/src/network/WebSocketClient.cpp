/**
 * Chrono-shift 客户端 WebSocket 封装
 * C++17 重构版 — 实现文件
 *
 * WebSocket (RFC 6455) 握手、帧编解码、控制帧处理
 */

#include "WebSocketClient.h"
#include "TcpConnection.h"
#include "Sha1.h"

#include <cstring>
#include <cstdlib>
#include <ctime>

#include <winsock2.h>
#include <windows.h>
#include <wincrypt.h>

#pragma comment(lib, "crypt32.lib")

#include "../../server/include/tls_server.h"

#include "../util/Logger.h"

namespace chrono {
namespace client {
namespace network {

/* ============================================================
 * 常量
 * ============================================================ */

static constexpr size_t kKeyLen    = 24;  /* base64(16 bytes) = 24 chars */
static constexpr size_t kAcceptLen = 28;  /* base64(sha1(20 bytes)) = 28 chars */
static const char*      kGuid      = "258EAFA5-E914-47DA-95CA-5AB5E9A1DA32";

/* ============================================================
 * 构造函数
 * ============================================================ */

WebSocketClient::WebSocketClient()
    : tcp_(nullptr)
{
}

/* ============================================================
 * 生成随机的 Sec-WebSocket-Key
 * ============================================================ */

bool WebSocketClient::generate_key(char* key_b64, size_t key_len)
{
    if (!key_b64 || key_len < kKeyLen + 1) {
        return false;
    }

    unsigned char random_key[16];

    /* 使用 Windows CryptoAPI */
    HCRYPTPROV hProv = 0;
    if (CryptAcquireContext(&hProv, nullptr, nullptr, PROV_RSA_FULL,
                            CRYPT_VERIFYCONTEXT)) {
        CryptGenRandom(hProv, 16, random_key);
        CryptReleaseContext(hProv, 0);
    } else {
        /* 回退: 使用时间和 rand() */
        std::srand(static_cast<unsigned int>(std::time(nullptr)));
        for (int i = 0; i < 16; i++) {
            random_key[i] = static_cast<unsigned char>(std::rand() & 0xFF);
        }
    }

    Sha1::base64_encode(random_key, 16, key_b64);
    return true;
}

/* ============================================================
 * WebSocket 握手
 * ============================================================ */

bool WebSocketClient::handshake(TcpConnection& tcp,
                                const std::string& host,
                                const std::string& path)
{
    if (!tcp.is_connected()) {
        LOG_ERROR("WebSocket 握手失败: TCP 未连接");
        return false;
    }

    tcp_ = &tcp;

    /* 生成随机的 Sec-WebSocket-Key */
    char key_b64[kKeyLen + 1] = {};
    if (!generate_key(key_b64, sizeof(key_b64))) {
        LOG_ERROR("WebSocket 握手失败: 无法生成 Key");
        return false;
    }

    /* 计算期望的 Sec-WebSocket-Accept */
    std::string concat = std::string(key_b64) + kGuid;
    unsigned char sha1_digest[20];
    {
        Sha1 sha1;
        sha1.init();
        sha1.update(reinterpret_cast<const uint8_t*>(concat.data()), concat.size());
        sha1.final(sha1_digest);
    }

    char expected_accept[kAcceptLen + 1] = {};
    Sha1::base64_encode(sha1_digest, 20, expected_accept);

    /* 构建 HTTP Upgrade 请求 */
    std::string request =
        "GET " + path + " HTTP/1.1\r\n"
        "Host: " + host + "\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: " + key_b64 + "\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n";

    /* 发送请求 */
    if (tcp_->send_all(reinterpret_cast<const uint8_t*>(request.data()),
                       request.size()) != 0) {
        LOG_ERROR("发送 WebSocket 握手请求失败");
        return false;
    }

    /* 接收响应 (逐字节直到找到 \r\n\r\n) */
    std::string response;
    response.reserve(1024);
    uint8_t byte;

    while (response.size() < 4096) {
        if (tcp_->recv_all(&byte, 1) != 0) {
            LOG_ERROR("WebSocket 握手响应接收失败");
            return false;
        }
        response.push_back(static_cast<char>(byte));

        /* 检查是否收到完整的响应头 */
        if (response.size() >= 4 &&
            response.compare(response.size() - 4, 4, "\r\n\r\n") == 0) {
            break;
        }
    }

    LOG_DEBUG("WebSocket 响应:\n%s", response.c_str());

    /* 验证状态码 101 */
    if (response.find("101") == std::string::npos) {
        LOG_ERROR("WebSocket 握手失败: 状态码非 101");
        LOG_DEBUG("响应: %s", response.c_str());
        return false;
    }

    /* 验证 Upgrade */
    if (response.find("Upgrade: websocket") == std::string::npos &&
        response.find("upgrade: websocket") == std::string::npos) {
        LOG_ERROR("WebSocket 握手失败: 缺少 Upgrade 头");
        return false;
    }

    /* 提取并验证 Sec-WebSocket-Accept */
    const char accept_header[] = "Sec-WebSocket-Accept:";
    auto accept_pos = response.find(accept_header);
    if (accept_pos == std::string::npos) {
        LOG_ERROR("WebSocket 握手失败: 缺少 Sec-WebSocket-Accept");
        return false;
    }

    auto value_start = accept_pos + sizeof(accept_header) - 1;
    /* 跳过空白 */
    while (value_start < response.size() &&
           (response[value_start] == ' ' || response[value_start] == '\t')) {
        value_start++;
    }

    /* 提取到行尾 */
    auto value_end = response.find("\r\n", value_start);
    if (value_end == std::string::npos) {
        value_end = response.size();
    }

    std::string received_accept = response.substr(value_start, value_end - value_start);

    if (received_accept != expected_accept) {
        LOG_ERROR("WebSocket 握手失败: Sec-WebSocket-Accept 不匹配");
        LOG_DEBUG("期望: %s", expected_accept);
        LOG_DEBUG("收到: %s", received_accept.c_str());
        return false;
    }

    LOG_INFO("WebSocket 连接成功: %s", path.c_str());
    return true;
}

/* ============================================================
 * 发送 WebSocket 帧 (带掩码, RFC 6455 §5.1)
 * ============================================================ */

int WebSocketClient::send_frame(WsOpcode opcode, const uint8_t* payload, size_t len)
{
    if (!tcp_ || !tcp_->is_connected()) {
        LOG_ERROR("WebSocket 发送失败: 未连接");
        return -1;
    }
    if (!payload && len > 0) {
        return -1;
    }

    /* 构建帧头 */
    /* Byte 0: FIN=1, RSV=0, Opcode */
    uint8_t header[10];  /* 最多 10 字节头部 */
    size_t header_len;

    header[0] = 0x80 | static_cast<uint8_t>(opcode);  /* FIN + Opcode */

    /* Byte 1+: 掩码位=1 + 长度编码 (客户端必须掩码) */
    if (len < 126) {
        header[1] = 0x80 | static_cast<uint8_t>(len);
        header_len = 2;
    } else if (len <= 65535) {
        header[1] = 0x80 | 126;
        header[2] = static_cast<uint8_t>((len >> 8) & 0xFF);
        header[3] = static_cast<uint8_t>(len & 0xFF);
        header_len = 4;
    } else {
        header[1] = 0x80 | 127;
        uint64_t len64 = static_cast<uint64_t>(len);
        for (int i = 7; i >= 0; i--) {
            header[2 + (7 - i)] = static_cast<uint8_t>((len64 >> (i * 8)) & 0xFF);
        }
        header_len = 10;
    }

    /* 生成掩码 key */
    unsigned char mask_key[4];
    HCRYPTPROV hProv = 0;
    if (CryptAcquireContext(&hProv, nullptr, nullptr, PROV_RSA_FULL,
                            CRYPT_VERIFYCONTEXT)) {
        CryptGenRandom(hProv, 4, mask_key);
        CryptReleaseContext(hProv, 0);
    } else {
        mask_key[0] = static_cast<unsigned char>(std::rand() & 0xFF);
        mask_key[1] = static_cast<unsigned char>(std::rand() & 0xFF);
        mask_key[2] = static_cast<unsigned char>(std::rand() & 0xFF);
        mask_key[3] = static_cast<unsigned char>(std::rand() & 0xFF);
    }

    /* 追加掩码 key 到头部 */
    for (int i = 0; i < 4; i++) {
        header[header_len + i] = mask_key[i];
    }
    header_len += 4;

    /* 分配发送缓冲区 = header + 掩码后的数据 */
    std::vector<uint8_t> send_buf(header_len + len);
    std::memcpy(send_buf.data(), header, header_len);

    /* 对 payload 做掩码处理 */
    if (len > 0) {
        for (size_t i = 0; i < len; i++) {
            send_buf[header_len + i] = payload[i] ^ mask_key[i % 4];
        }
    }

    /* 发送 */
    if (tcp_->send_all(send_buf.data(), send_buf.size()) != 0) {
        LOG_ERROR("WebSocket 发送失败");
        return -1;
    }

    return 0;
}

/* ============================================================
 * 接收 WebSocket 帧
 * ============================================================ */

int WebSocketClient::recv_frame(WsFrame& frame)
{
    if (!tcp_ || !tcp_->is_connected()) {
        LOG_ERROR("WebSocket 接收失败: 未连接");
        return -1;
    }

    frame.payload.clear();

    /* --- 读取 Byte 0 (FIN + RSV + Opcode) --- */
    uint8_t byte0;
    if (tcp_->recv_all(&byte0, 1) != 0) {
        return -1;
    }

    uint8_t opcode_val = byte0 & 0x0F;
    /* fin: (byte0 >> 7) & 0x01 — 暂不处理分片 */

    /* 处理控制帧 */
    if (opcode_val == 0x8) { /* Close */
        uint8_t close_hdr[2];
        if (tcp_->recv_all(close_hdr, 2) != 0) return -1;
        uint16_t close_code = (static_cast<uint16_t>(close_hdr[0]) << 8) | close_hdr[1];
        LOG_DEBUG("WebSocket 收到 Close 帧, code=%u", close_code);
        return -1; /* 连接关闭 */
    } else if (opcode_val == 0x9) { /* Ping */
        /* 回复 Pong */
        uint8_t pong[] = { 0x8A, 0x00 };
        tcp_->send_all(pong, 2);
        return recv_frame(frame); /* 递归读取下一帧 */
    } else if (opcode_val == 0xA) { /* Pong */
        /* 忽略 Pong，继续读取 */
        uint8_t ignore_hdr;
        tcp_->recv_all(&ignore_hdr, 1);
        return recv_frame(frame);
    }

    /* --- 读取 Byte 1 (Mask + Payload Length) --- */
    uint8_t byte1;
    if (tcp_->recv_all(&byte1, 1) != 0) {
        return -1;
    }

    bool masked           = (byte1 >> 7) & 0x01;
    uint64_t payload_len  = byte1 & 0x7F;

    /* 扩展长度 */
    if (payload_len == 126) {
        uint8_t ext[2];
        if (tcp_->recv_all(ext, 2) != 0) return -1;
        payload_len = (static_cast<uint64_t>(ext[0]) << 8) | ext[1];
    } else if (payload_len == 127) {
        uint8_t ext[8];
        if (tcp_->recv_all(ext, 8) != 0) return -1;
        payload_len = 0;
        for (int i = 0; i < 8; i++) {
            payload_len = (payload_len << 8) | ext[i];
        }
    }

    if (payload_len > 1024 * 1024) { /* 限制 1MB */
        LOG_ERROR("WebSocket 帧过大: %llu bytes",
                  static_cast<unsigned long long>(payload_len));
        return -1;
    }

    /* 读取掩码 key (如果有) */
    unsigned char mask_key[4] = {};
    if (masked) {
        if (tcp_->recv_all(mask_key, 4) != 0) return -1;
    }

    /* 读取 payload */
    frame.payload.resize(static_cast<size_t>(payload_len));
    if (payload_len > 0) {
        if (tcp_->recv_all(frame.payload.data(), static_cast<size_t>(payload_len)) != 0) {
            return -1;
        }

        /* 如果服务端发了掩码，解除掩码 */
        if (masked) {
            for (size_t i = 0; i < static_cast<size_t>(payload_len); i++) {
                frame.payload[i] ^= mask_key[i % 4];
            }
        }
    }

    frame.opcode = static_cast<WsOpcode>(opcode_val);
    frame.mask   = masked;
    return 0;
}

} // namespace network
} // namespace client
} // namespace chrono
