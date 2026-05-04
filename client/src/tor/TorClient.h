#pragma once
/**
 * TorClient.h — Tor SOCKS5 代理 + Control Protocol 客户端
 *
 * Tor 集成 (默认传输层):
 *   SOCKS5 代理: localhost:9050 (出站连接)
 *   ControlPort: localhost:9051 (状态/控制)
 *
 * I2P 为备选传输层 (见 src/i2p/SamClient.h)
 */
#include <string>
#include <cstdint>
#include <vector>

namespace chrono { namespace client { namespace tor {

struct TorStatus {
    bool connected = false;
    std::string version;
    std::string nickname;
    std::string fingerprint;
    int circuits_active = 0;
    int circuits_total = 0;
    uint64_t bytes_read = 0;
    uint64_t bytes_written = 0;
};

struct TorCircuit {
    std::string id;
    std::string status;       // BUILT, EXTENDED, FAILED, CLOSED
    std::string path;         // $fingerprint~$fingerprint...
    std::string purpose;      // GENERAL, HS_CLIENT_HSDIR, HS_SERVICE_HSDIR...
};

class TorClient {
public:
    TorClient() = default;
    ~TorClient();

    // === SOCKS5 代理 ===

    /** 连接到 Tor SOCKS5 代理 (默认 localhost:9050) */
    bool connect_socks(const std::string& host = "127.0.0.1", uint16_t port = 9050);

    /** 通过 SOCKS5 连接到目标 onion 地址 */
    int socks_connect(const std::string& onion_addr, uint16_t port);

    /** 通过当前 SOCKS 连接发送数据 */
    int socks_send(const uint8_t* data, size_t len);

    /** 通过当前 SOCKS 连接接收数据 */
    int socks_recv(uint8_t* buf, size_t max_len);

    /** 关闭当前 SOCKS 连接 */
    void socks_close();

    // === Control Protocol (localhost:9051) ===

    /** 连接到 Tor ControlPort 并认证 */
    bool connect_control(const std::string& host = "127.0.0.1", uint16_t port = 9051,
                         const std::string& password = "");

    /** 获取 Tor 状态信息 */
    TorStatus get_status();

    /** 获取活跃电路列表 */
    std::vector<TorCircuit> get_circuits();

    /** 发送控制命令并获取响应 */
    std::string control_command(const std::string& cmd);

    /** 断开 ControlPort */
    void control_disconnect();

    // === 通用 ===

    /** Tor 代理是否可用 */
    bool is_socks_ready() const { return socks_fd_ >= 0; }

    /** ControlPort 是否已连接 */
    bool is_control_ready() const { return ctrl_fd_ >= 0; }

    /** 完全断开所有连接 */
    void disconnect_all();

    /** 获取错误信息 */
    const std::string& last_error() const { return last_error_; }

private:
    // SOCKS5 握手
    bool socks5_handshake();

    // Control Protocol 认证
    bool control_authenticate(const std::string& password);

    int socks_fd_ = -1;
    int ctrl_fd_ = -1;
    std::string last_error_;
};

} } } // namespace
