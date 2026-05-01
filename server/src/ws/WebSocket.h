/**
 * Chrono-shift C++ WebSocket 模块
 * WebSocket 帧编解码与握手
 * C++17 重构版
 */
#ifndef CHRONO_CPP_WEBSOCKET_H
#define CHRONO_CPP_WEBSOCKET_H

#include <string>
#include <vector>
#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>

namespace chrono {
namespace ws {

// WebSocket 操作码
enum class WsOpcode : uint8_t {
    kContinuation = 0x0,
    kText         = 0x1,
    kBinary       = 0x2,
    kClose        = 0x8,
    kPing         = 0x9,
    kPong         = 0xA
};

// WebSocket 帧头
struct WsFrameHeader {
    bool    fin;
    bool    mask;
    WsOpcode opcode;
    uint8_t mask_key[4];
    uint64_t payload_length;
};

/**
 * WebSocket 帧
 */
struct WsFrame {
    WsFrameHeader header;
    std::vector<uint8_t> payload;
};

/**
 * WebSocket 连接状态
 */
enum class WsState {
    kConnecting,    // 握手进行中
    kOpen,          // 连接已建立
    kClosing,       // 关闭中
    kClosed         // 已关闭
};

/**
 * WebSocket 连接
 */
struct WsConnection {
    int fd = -1;
    WsState state = WsState::kConnecting;
    void* ssl = nullptr;  // SSL* 或 nullptr (非 TLS)
    std::string path;
    std::unordered_map<std::string, std::string> headers;

    // 接收缓冲区
    std::vector<uint8_t> read_buf;
    // 部分帧数据 (分片)
    std::vector<uint8_t> fragmented_data;
    WsOpcode fragmented_opcode = WsOpcode::kText;
};

/**
 * WebSocket 管理器
 */
class WebSocketManager {
public:
    WebSocketManager() = default;
    ~WebSocketManager();

    // 禁止拷贝
    WebSocketManager(const WebSocketManager&) = delete;
    WebSocketManager& operator=(const WebSocketManager&) = delete;

    /**
     * 处理 HTTP Upgrade 请求
     * @param fd socket 文件描述符
     * @param ssl SSL 对象指针 (可能为 nullptr)
     * @param path 请求路径
     * @param headers HTTP 请求头
     * @return 连接 ID，失败返回 -1
     */
    int handle_upgrade(int fd, void* ssl, const std::string& path,
                       const std::unordered_map<std::string, std::string>& headers);

    /**
     * 发送帧
     */
    bool send_frame(int conn_id, const WsFrame& frame);
    bool send_text(int conn_id, const std::string& text);
    bool send_binary(int conn_id, const std::vector<uint8_t>& data);
    bool send_ping(int conn_id);
    bool send_pong(int conn_id);
    bool send_close(int conn_id, uint16_t code = 1000);

    /**
     * 接收并解析帧
     * @return 解析出的帧列表 (可能包含多个)
     */
    std::vector<WsFrame> recv_frames(int conn_id, const uint8_t* data, size_t len);

    /**
     * 关闭连接
     */
    void close_connection(int conn_id);

    /**
     * 获取连接
     */
    WsConnection* get_connection(int conn_id);

    /**
     * 事件回调
     */
    using OnMessageFn = std::function<void(int conn_id, const WsFrame& frame)>;
    using OnCloseFn = std::function<void(int conn_id, uint16_t code)>;

    void set_on_message(OnMessageFn fn) { on_message_ = std::move(fn); }
    void set_on_close(OnCloseFn fn) { on_close_ = std::move(fn); }

public:
    // SHA-1 哈希 (用于 WebSocket 握手)
    static std::string sha1(const std::string& input);
    static std::string base64_encode(const std::string& input);

private:
    std::unordered_map<int, std::unique_ptr<WsConnection>> connections_;
    int next_id_ = 1;
    OnMessageFn on_message_;
    OnCloseFn on_close_;

    // 握手
    bool perform_handshake(WsConnection* conn,
                           const std::unordered_map<std::string, std::string>& headers);

    // 帧编解码
    std::vector<uint8_t> encode_frame(const WsFrame& frame);
    std::vector<WsFrame> decode_frames(WsConnection* conn, const uint8_t* data, size_t len);
};

// ============================================================
// 便利函数
// ============================================================

/**
 * 创建 WebSocket 握手响应
 */
std::string create_websocket_accept(const std::string& key);

/**
 * 检查是否是 WebSocket Upgrade 请求
 */
bool is_websocket_upgrade(const std::unordered_map<std::string, std::string>& headers);

} // namespace ws
} // namespace chrono

#endif // CHRONO_CPP_WEBSOCKET_H
