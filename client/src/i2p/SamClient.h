#pragma once
/**
 * SamClient.h — I2P SAM v3 API 客户端 (支持本地模拟模式)
 *
 * SAM (Simple Anonymous Messaging) 桥接协议:
 * - 生产模式: 连接 I2P 路由器 localhost:7656
 * - 模拟模式: 本地 TCP 回环, 用于开发测试
 */
#include <string>
#include <cstdint>
#include <functional>

namespace chrono { namespace client { namespace i2p {

struct SamSession {
    std::string session_id;
    std::string destination;   // 我们的 .b32.i2p 地址
    bool active = false;
};

class SamClient {
public:
    SamClient() = default;
    ~SamClient();

    // 连接到 SAM 桥接 (本地模式用本地端口)
    bool connect(const std::string& host = "127.0.0.1", uint16_t port = 7656);

    // 创建会话
    bool create_session(const std::string& session_id = "chrono");

    // 地址解析
    std::string naming_lookup(const std::string& b32_addr);

    // 打开到目标对等端的流
    bool stream_connect(const std::string& destination);

    // 在一个流上发送原始数据
    int stream_send(const uint8_t* data, size_t len);

    // 从一个流上接收原始数据
    int stream_recv(uint8_t* buf, size_t max_len);

    // 关闭流
    void stream_close();

    // 断开连接
    void disconnect();

    // 查询
    const std::string& our_destination() const { return session_.destination; }
    bool is_connected() const { return session_.active; }

private:
    SamSession session_;
    int sam_fd_ = -1;
    int stream_fd_ = -1;
    bool local_mode_ = false;  // 模拟模式标志
};

} } } // namespace chrono::client::i2p
