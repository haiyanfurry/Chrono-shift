/**
 * Chrono-shift 客户端 WebSocket 封装
 * C++17 重构版
 */
#ifndef CHRONO_CLIENT_WEBSOCKET_CLIENT_H
#define CHRONO_CLIENT_WEBSOCKET_CLIENT_H

#include <cstdint>
#include <string>
#include <vector>

namespace chrono {
namespace client {
namespace network {

class TcpConnection;
class Sha1;

/**
 * WebSocket 客户端 (RFC 6455)
 * 处理握手、帧编解码、控制帧
 */
class WebSocketClient {
public:
    /** WebSocket Opcode */
    enum class WsOpcode : uint8_t {
        kContinuation = 0x0,
        kText         = 0x1,
        kBinary       = 0x2,
        kClose        = 0x8,
        kPing         = 0x9,
        kPong         = 0xA
    };

    /** WebSocket 帧结构 */
    struct WsFrame {
        WsOpcode opcode = WsOpcode::kBinary;
        bool mask = false;
        std::vector<uint8_t> payload;
    };

    WebSocketClient();

    /**
     * WebSocket 握手
     * @param tcp TCP 连接 (需已连接)
     * @param host 服务器主机名
     * @param path 请求路径 (如 "/ws")
     * @return true=握手成功
     */
    bool handshake(TcpConnection& tcp,
                   const std::string& host,
                   const std::string& path);

    /**
     * 发送 WebSocket 帧
     * @param opcode 帧类型
     * @param payload 负载数据
     * @param len 负载长度
     * @return 0=成功, -1=失败
     */
    int send_frame(WsOpcode opcode, const uint8_t* payload, size_t len);

    /**
     * 接收 WebSocket 帧
     * @param frame 输出: 接收到的帧
     * @return 0=成功, -1=失败
     */
    int recv_frame(WsFrame& frame);

private:
    /** 生成随机的 Sec-WebSocket-Key */
    bool generate_key(char* key_b64, size_t key_len);

    /** 当前 TCP 连接 (在 handshake() 中设置) */
    TcpConnection* tcp_;
};

} // namespace network
} // namespace client
} // namespace chrono

#endif // CHRONO_CLIENT_WEBSOCKET_CLIENT_H
