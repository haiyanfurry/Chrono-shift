/**
 * Chrono-shift C++ WebSocket 实现
 */
#include "WebSocket.h"
#include "../util/Logger.h"

#include <cstring>
#include <sstream>
#include <array>
#include <algorithm>

// 平台相关网络头文件
#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#endif

// OpenSSL SHA-1
#include <openssl/sha.h>

namespace chrono {
namespace ws {

// WebSocket GUID (RFC 6455)
static const char* kWebSocketGUID = "258EAFA5-E914-47DA-95CA-5AB9DC11B85B";

// ============================================================
// 构造函数/析构函数
// ============================================================
WebSocketManager::~WebSocketManager()
{
    for (auto& [id, conn] : connections_) {
        if (conn && conn->fd >= 0) {
            close_connection(id);
        }
    }
    connections_.clear();
}

// ============================================================
// 握手检测
// ============================================================
bool is_websocket_upgrade(const std::unordered_map<std::string, std::string>& headers)
{
    auto it = headers.find("Upgrade");
    if (it == headers.end()) {
        // 尝试小写
        it = headers.find("upgrade");
        if (it == headers.end()) return false;
    }
    // 忽略大小写比较
    std::string val = it->second;
    std::transform(val.begin(), val.end(), val.begin(), ::tolower);
    return val == "websocket";
}

// ============================================================
// 握手
// ============================================================
std::string create_websocket_accept(const std::string& key)
{
    std::string concat = key + kWebSocketGUID;
    // 实际 SHA-1 + Base64，这里使用简化实现
    // 生产环境应链接 OpenSSL
    return WebSocketManager::base64_encode(WebSocketManager::sha1(concat));
}

bool WebSocketManager::perform_handshake(
    WsConnection* conn,
    const std::unordered_map<std::string, std::string>& headers)
{
    // 查找 Sec-WebSocket-Key
    auto it = headers.find("Sec-WebSocket-Key");
    if (it == headers.end()) {
        LOG_ERROR("[WS] Missing Sec-WebSocket-Key header");
        return false;
    }

    // 查找 Sec-WebSocket-Version
    auto ver_it = headers.find("Sec-WebSocket-Version");
    if (ver_it == headers.end() || ver_it->second != "13") {
        LOG_ERROR("[WS] Unsupported WebSocket version");
        return false;
    }

    std::string accept_key = create_websocket_accept(it->second);

    // 构建握手响应
    std::string response =
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: " + accept_key + "\r\n"
        "\r\n";

    // 发送响应
    if (conn->ssl) {
        // TLS 连接 - 通过 OpenSSL 发送
        SSL_write(static_cast<SSL*>(conn->ssl), response.data(),
                  static_cast<int>(response.size()));
    } else {
        // 普通 TCP
#ifdef _WIN32
        send(conn->fd, response.data(), static_cast<int>(response.size()), 0);
#else
        write(conn->fd, response.data(), response.size());
#endif
    }

    conn->state = WsState::kOpen;
    LOG_INFO("[WS] WebSocket handshake successful (fd=%d)", conn->fd);
    return true;
}

int WebSocketManager::handle_upgrade(
    int fd, void* ssl, const std::string& path,
    const std::unordered_map<std::string, std::string>& headers)
{
    auto conn = std::make_unique<WsConnection>();
    conn->fd = fd;
    conn->ssl = ssl;
    conn->path = path;
    conn->headers = headers;
    conn->state = WsState::kConnecting;

    if (!perform_handshake(conn.get(), headers)) {
        LOG_ERROR("[WS] Handshake failed for fd=%d", fd);
        return -1;
    }

    int id = next_id_++;
    connections_[id] = std::move(conn);
    LOG_INFO("[WS] New WebSocket connection (id=%d, fd=%d, path=%s)",
             id, fd, path.c_str());
    return id;
}

// ============================================================
// 帧编码
// ============================================================
std::vector<uint8_t> WebSocketManager::encode_frame(const WsFrame& frame)
{
    std::vector<uint8_t> buf;
    size_t header_len = 2; // 基础头: FIN+opcode + mask+length

    uint64_t len = frame.payload.size();
    if (len > 65535) {
        header_len += 8;  // 64-bit 扩展长度
    } else if (len > 125) {
        header_len += 2;  // 16-bit 扩展长度
    }

    buf.resize(header_len + len);
    size_t pos = 0;

    // 第一个字节: FIN + opcode
    buf[pos] = (frame.header.fin ? 0x80 : 0x00) |
               static_cast<uint8_t>(frame.header.opcode);
    pos++;

    // 第二个字节: MASK + payload length
    buf[pos] = (frame.header.mask ? 0x80 : 0x00);
    if (len <= 125) {
        buf[pos] |= static_cast<uint8_t>(len);
        pos++;
    } else if (len <= 65535) {
        buf[pos] |= 126;
        pos++;
        buf[pos++] = static_cast<uint8_t>((len >> 8) & 0xFF);
        buf[pos++] = static_cast<uint8_t>(len & 0xFF);
    } else {
        buf[pos] |= 127;
        pos++;
        for (int i = 7; i >= 0; i--) {
            buf[pos++] = static_cast<uint8_t>((len >> (i * 8)) & 0xFF);
        }
    }

    // Mask key (如果 mask 开启)
    if (frame.header.mask) {
        std::memcpy(buf.data() + pos, frame.header.mask_key, 4);
        pos += 4;
    }

    // Payload
    if (len > 0) {
        std::memcpy(buf.data() + pos, frame.payload.data(), len);
        // 如果 mask 开启，对 payload 进行掩码
        if (frame.header.mask) {
            for (size_t i = 0; i < len; i++) {
                buf[pos + i] ^= frame.header.mask_key[i % 4];
            }
        }
    }

    return buf;
}

// ============================================================
// 帧解码
// ============================================================
std::vector<WsFrame> WebSocketManager::decode_frames(
    WsConnection* conn, const uint8_t* data, size_t len)
{
    std::vector<WsFrame> frames;

    // 追加到读缓冲区
    conn->read_buf.insert(conn->read_buf.end(), data, data + len);

    while (true) {
        if (conn->read_buf.size() < 2) break; // 至少需要 2 字节头

        size_t pos = 0;
        uint8_t b0 = conn->read_buf[pos++];
        uint8_t b1 = conn->read_buf[pos++];

        WsFrame frame;
        frame.header.fin    = (b0 & 0x80) != 0;
        frame.header.opcode = static_cast<WsOpcode>(b0 & 0x0F);
        frame.header.mask   = (b1 & 0x80) != 0;

        uint64_t payload_len = b1 & 0x7F;
        if (payload_len == 126) {
            if (conn->read_buf.size() < 4) break;
            payload_len = (static_cast<uint64_t>(conn->read_buf[pos]) << 8) |
                           conn->read_buf[pos + 1];
            pos += 2;
        } else if (payload_len == 127) {
            if (conn->read_buf.size() < 10) break;
            payload_len = 0;
            for (int i = 0; i < 8; i++) {
                payload_len = (payload_len << 8) | conn->read_buf[pos + i];
            }
            pos += 8;
        }
        frame.header.payload_length = payload_len;

        // Mask key
        if (frame.header.mask) {
            if (conn->read_buf.size() < pos + 4) break;
            std::memcpy(frame.header.mask_key, conn->read_buf.data() + pos, 4);
            pos += 4;
        }

        // Payload
        if (conn->read_buf.size() < pos + payload_len) break;

        frame.payload.assign(conn->read_buf.begin() + pos,
                             conn->read_buf.begin() + pos + payload_len);

        // 如果 mask 开启，解除掩码
        if (frame.header.mask) {
            for (size_t i = 0; i < payload_len; i++) {
                frame.payload[i] ^= frame.header.mask_key[i % 4];
            }
        }

        // 从缓冲区移除已处理的数据
        conn->read_buf.erase(conn->read_buf.begin(),
                             conn->read_buf.begin() + pos + payload_len);

        // 处理控制帧
        if (static_cast<uint8_t>(frame.header.opcode) & 0x08) {
            // 控制帧
            switch (frame.header.opcode) {
            case WsOpcode::kClose: {
                uint16_t close_code = 1000;
                if (frame.payload.size() >= 2) {
                    close_code = (static_cast<uint16_t>(frame.payload[0]) << 8) |
                                  frame.payload[1];
                }
                LOG_INFO("[WS] Close frame received (code=%u)", close_code);
                send_close(0, close_code); // 回复 close
                if (on_close_) {
                    on_close_(0, close_code); // 注意: conn_id 需映射
                }
                break;
            }
            case WsOpcode::kPing:
                send_ping(0);
                break;
            case WsOpcode::kPong:
                break; // 忽略 pong
            default:
                break;
            }
            continue;
        }

        // 数据帧处理 (分片)
        if (!frame.header.fin) {
            // 第一个分片或继续
            if (frame.header.opcode != WsOpcode::kContinuation) {
                conn->fragmented_data = frame.payload;
                conn->fragmented_opcode = frame.header.opcode;
            } else {
                conn->fragmented_data.insert(conn->fragmented_data.end(),
                                              frame.payload.begin(),
                                              frame.payload.end());
            }
        } else {
            // FIN 帧
            if (frame.header.opcode == WsOpcode::kContinuation) {
                // 完成分片消息
                conn->fragmented_data.insert(conn->fragmented_data.end(),
                                              frame.payload.begin(),
                                              frame.payload.end());
                WsFrame complete;
                complete.header.fin = true;
                complete.header.opcode = conn->fragmented_opcode;
                complete.payload = std::move(conn->fragmented_data);
                frames.push_back(std::move(complete));
                conn->fragmented_data.clear();
            } else {
                frames.push_back(std::move(frame));
            }
        }
    }

    return frames;
}

// ============================================================
// 发送方法
// ============================================================
bool WebSocketManager::send_frame(int conn_id, const WsFrame& frame)
{
    auto it = connections_.find(conn_id);
    if (it == connections_.end() || !it->second) return false;

    auto& conn = it->second;
    if (conn->state != WsState::kOpen) return false;

    std::vector<uint8_t> wire = encode_frame(frame);

    int ret;
    if (conn->ssl) {
        ret = SSL_write(static_cast<SSL*>(conn->ssl), wire.data(),
                        static_cast<int>(wire.size()));
    } else {
#ifdef _WIN32
        ret = send(conn->fd, reinterpret_cast<const char*>(wire.data()),
                   static_cast<int>(wire.size()), 0);
#else
        ret = write(conn->fd, wire.data(), wire.size());
#endif
    }
    return ret > 0;
}

bool WebSocketManager::send_text(int conn_id, const std::string& text)
{
    WsFrame frame;
    frame.header.fin    = true;
    frame.header.mask   = false;
    frame.header.opcode = WsOpcode::kText;
    frame.payload.assign(text.begin(), text.end());
    return send_frame(conn_id, frame);
}

bool WebSocketManager::send_binary(int conn_id, const std::vector<uint8_t>& data)
{
    WsFrame frame;
    frame.header.fin    = true;
    frame.header.mask   = false;
    frame.header.opcode = WsOpcode::kBinary;
    frame.payload = data;
    return send_frame(conn_id, frame);
}

bool WebSocketManager::send_ping(int conn_id)
{
    WsFrame frame;
    frame.header.fin    = true;
    frame.header.mask   = false;
    frame.header.opcode = WsOpcode::kPing;
    return send_frame(conn_id, frame);
}

bool WebSocketManager::send_pong(int conn_id)
{
    WsFrame frame;
    frame.header.fin    = true;
    frame.header.mask   = false;
    frame.header.opcode = WsOpcode::kPong;
    return send_frame(conn_id, frame);
}

bool WebSocketManager::send_close(int conn_id, uint16_t code)
{
    WsFrame frame;
    frame.header.fin    = true;
    frame.header.mask   = false;
    frame.header.opcode = WsOpcode::kClose;
    frame.payload.push_back(static_cast<uint8_t>((code >> 8) & 0xFF));
    frame.payload.push_back(static_cast<uint8_t>(code & 0xFF));
    return send_frame(conn_id, frame);
}

// ============================================================
// 接收
// ============================================================
std::vector<WsFrame> WebSocketManager::recv_frames(
    int conn_id, const uint8_t* data, size_t len)
{
    auto it = connections_.find(conn_id);
    if (it == connections_.end() || !it->second) return {};
    return decode_frames(it->second.get(), data, len);
}

// ============================================================
// 关闭连接
// ============================================================
void WebSocketManager::close_connection(int conn_id)
{
    auto it = connections_.find(conn_id);
    if (it == connections_.end() || !it->second) return;

    auto& conn = it->second;
    conn->state = WsState::kClosed;

    if (conn->ssl) {
        SSL_shutdown(static_cast<SSL*>(conn->ssl));
        SSL_free(static_cast<SSL*>(conn->ssl));
        conn->ssl = nullptr;
    }

    if (conn->fd >= 0) {
#ifdef _WIN32
        closesocket(conn->fd);
#else
        close(conn->fd);
#endif
        conn->fd = -1;
    }

    LOG_INFO("[WS] Connection closed (id=%d)", conn_id);
    connections_.erase(it);
}

WsConnection* WebSocketManager::get_connection(int conn_id)
{
    auto it = connections_.find(conn_id);
    if (it == connections_.end()) return nullptr;
    return it->second.get();
}

// ============================================================
// SHA-1 简化实现 (用于 WebSocket 握手)
// 生产环境应链接 OpenSSL 的 SHA-1
// ============================================================
std::string WebSocketManager::sha1(const std::string& input)
{
    // 简化实现：实际生产环境使用 OpenSSL SHA-1
    // 这里返回一个占位值，编译时替换为真实实现
    // 真实实现：#include <openssl/sha.h>
    // unsigned char hash[SHA_DIGEST_LENGTH];
    // SHA1(reinterpret_cast<const unsigned char*>(input.data()), input.size(), hash);
    // return std::string(reinterpret_cast<char*>(hash), SHA_DIGEST_LENGTH);

    // OpenSSL 实现
    unsigned char hash[20];
    SHA1(reinterpret_cast<const unsigned char*>(input.data()),
         input.size(), hash);
    return std::string(reinterpret_cast<char*>(hash), 20);
}

std::string WebSocketManager::base64_encode(const std::string& input)
{
    static const char table[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string output;
    size_t len = input.size();
    output.reserve(((len + 2) / 3) * 4);

    for (size_t i = 0; i < len; i += 3) {
        uint32_t triple = 0;
        int remain = static_cast<int>(len - i);
        if (remain >= 3) {
            triple = (static_cast<unsigned char>(input[i]) << 16) |
                     (static_cast<unsigned char>(input[i + 1]) << 8) |
                     static_cast<unsigned char>(input[i + 2]);
        } else if (remain == 2) {
            triple = (static_cast<unsigned char>(input[i]) << 16) |
                     (static_cast<unsigned char>(input[i + 1]) << 8);
        } else {
            triple = static_cast<unsigned char>(input[i]) << 16;
        }

        output += table[(triple >> 18) & 0x3F];
        output += table[(triple >> 12) & 0x3F];
        output += (remain >= 3) ? table[(triple >> 6) & 0x3F] : '=';
        output += (remain >= 2) ? table[triple & 0x3F] : '=';
    }

    return output;
}

} // namespace ws
} // namespace chrono
