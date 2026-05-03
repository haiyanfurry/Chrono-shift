/**
 * cmd_ws.cpp - WebSocket 调试命令 (C++23)
 * 对应 cmd_ws.c 的 C++23 重构版
 * 含 SHA-1 实现 (用于 WebSocket 握手)
 */
#include "../devtools_cli.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <print>
#include <thread>

namespace cli = chrono::client::cli;

// ============================================================
// TLS C 函数 (由 tls_client.c 实现)
// ============================================================
extern "C" {
extern int tls_client_init(const char* cert_dir);
extern int tls_client_connect(void** ssl, const char* host, unsigned short port);
extern int tls_write(void* ssl, const char* data, size_t len);
extern int tls_read(void* ssl, char* buf, size_t len);
extern void tls_close(void* ssl);
extern const char* tls_last_error(void);
}

// ============================================================
// WS 常量
// ============================================================
constexpr int WS_SHA1_DIGEST_LEN = 20;
constexpr int WS_KEY_LEN = 24;
constexpr const char* WS_MAGIC_STRING = "258EAFA5-E914-47DA-95CA-5AB5DC11B735";

constexpr int WS_OPCODE_CONTINUATION = 0x0;
constexpr int WS_OPCODE_TEXT         = 0x1;
constexpr int WS_OPCODE_BINARY       = 0x2;
constexpr int WS_OPCODE_CLOSE        = 0x8;
constexpr int WS_OPCODE_PING         = 0x9;
constexpr int WS_OPCODE_PONG         = 0xA;

// ============================================================
// SHA-1 实现 (用于 WebSocket 握手)
// ============================================================
static uint32_t sha1_rotl(uint32_t x, int n) noexcept {
    return (x << n) | (x >> (32 - n));
}

static void sha1_transform(uint32_t state[5], const unsigned char block[64]) noexcept {
    uint32_t w[80];
    uint32_t a, b, c, d, e, temp;

    for (int i = 0; i < 16; i++) {
        w[i] = (static_cast<uint32_t>(block[i * 4]) << 24)
             | (static_cast<uint32_t>(block[i * 4 + 1]) << 16)
             | (static_cast<uint32_t>(block[i * 4 + 2]) << 8)
             | static_cast<uint32_t>(block[i * 4 + 3]);
    }
    for (int i = 16; i < 80; i++)
        w[i] = sha1_rotl(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1);

    a = state[0]; b = state[1]; c = state[2]; d = state[3]; e = state[4];
    for (int i = 0; i < 80; i++) {
        uint32_t f, k;
        if (i < 20)       { f = (b & c) | ((~b) & d); k = 0x5A827999; }
        else if (i < 40)  { f = b ^ c ^ d;            k = 0x6ED9EBA1; }
        else if (i < 60)  { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
        else              { f = b ^ c ^ d;            k = 0xCA62C1D6; }
        temp = sha1_rotl(a, 5) + f + e + k + w[i];
        e = d; d = c; c = sha1_rotl(b, 30); b = a; a = temp;
    }
    state[0] += a; state[1] += b; state[2] += c; state[3] += d; state[4] += e;
}

static void sha1_hash(const unsigned char* data, size_t len,
                      unsigned char out[20]) noexcept {
    uint32_t state[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
    uint64_t bit_len = static_cast<uint64_t>(len) * 8;
    size_t pos = 0;
    unsigned char block[64];

    while (len >= 64) {
        std::memcpy(block, data + pos, 64);
        sha1_transform(state, block);
        pos += 64; len -= 64;
    }

    std::memset(block, 0, 64);
    std::memcpy(block, data + pos, len);
    block[len] = 0x80;
    if (len >= 56) { sha1_transform(state, block); std::memset(block, 0, 64); }
    block[56] = static_cast<unsigned char>(bit_len >> 56);
    block[57] = static_cast<unsigned char>(bit_len >> 48);
    block[58] = static_cast<unsigned char>(bit_len >> 40);
    block[59] = static_cast<unsigned char>(bit_len >> 32);
    block[60] = static_cast<unsigned char>(bit_len >> 24);
    block[61] = static_cast<unsigned char>(bit_len >> 16);
    block[62] = static_cast<unsigned char>(bit_len >> 8);
    block[63] = static_cast<unsigned char>(bit_len);
    sha1_transform(state, block);

    for (int i = 0; i < 5; i++) {
        out[i*4]   = static_cast<unsigned char>(state[i] >> 24);
        out[i*4+1] = static_cast<unsigned char>(state[i] >> 16);
        out[i*4+2] = static_cast<unsigned char>(state[i] >> 8);
        out[i*4+3] = static_cast<unsigned char>(state[i]);
    }
}

static constexpr const char base64_table_ws[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void sha1_base64(const unsigned char hash[20], char out[29]) noexcept {
    int j = 0;
    for (int i = 0; i < 20; i += 3) {
        int remaining = 20 - i;
        uint32_t triple = (static_cast<uint32_t>(hash[i]) << 16);
        if (remaining > 1) triple |= (static_cast<uint32_t>(hash[i+1]) << 8);
        if (remaining > 2) triple |= static_cast<uint32_t>(hash[i+2]);

        out[j++] = base64_table_ws[(triple >> 18) & 0x3F];
        out[j++] = base64_table_ws[(triple >> 12) & 0x3F];
        out[j++] = (remaining > 1) ? base64_table_ws[(triple >> 6) & 0x3F] : '=';
        out[j++] = (remaining > 2) ? base64_table_ws[triple & 0x3F] : '=';
    }
    out[28] = 0;
}

// ============================================================
// WebSocket 连接管理
// ============================================================

static void ws_compute_accept(const char* key, char out[29]) noexcept {
    char concat[128];
    unsigned char hash[20];
    std::snprintf(concat, sizeof(concat), "%s%s", key, WS_MAGIC_STRING);
    sha1_hash(reinterpret_cast<const unsigned char*>(concat),
              std::strlen(concat), hash);
    sha1_base64(hash, out);
}

static int ws_connect(const char* token, const char* path) noexcept {
    if (cli::g_cli_config.ws_connected) {
        std::println("[-] WebSocket 已连接, 先使用 ws close 关闭");
        return -1;
    }

    char request[8192];
    char key_bytes[16];
    char key_b64[WS_KEY_LEN + 1] = {0};
    char accept_expected[29];
    char response[8192] = {0};

    /* 生成随机 key */
    std::srand(static_cast<unsigned int>(std::time(nullptr)));
    for (int i = 0; i < 16; i++)
        key_bytes[i] = static_cast<unsigned char>(std::rand() % 256);

    {
        int j = 0;
        for (int i = 0; i < 16; i += 3) {
            int remaining = 16 - i;
            uint32_t triple = (static_cast<uint32_t>(
                static_cast<unsigned char>(key_bytes[i])) << 16);
            if (remaining > 1)
                triple |= (static_cast<uint32_t>(
                    static_cast<unsigned char>(key_bytes[i+1])) << 8);
            if (remaining > 2)
                triple |= static_cast<unsigned char>(key_bytes[i+2]);
            key_b64[j++] = base64_table_ws[(triple >> 18) & 0x3F];
            key_b64[j++] = base64_table_ws[(triple >> 12) & 0x3F];
            key_b64[j++] = (remaining > 1)
                ? base64_table_ws[(triple >> 6) & 0x3F] : '=';
            key_b64[j++] = (remaining > 2)
                ? base64_table_ws[triple & 0x3F] : '=';
        }
        key_b64[WS_KEY_LEN] = 0;
    }

    ws_compute_accept(key_b64, accept_expected);

    /* 构建 WebSocket 升级请求 */
    const char* ws_path = path ? path : "/api/ws";
    int len = std::snprintf(request, sizeof(request),
        "GET %s HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: %s\r\n"
        "Sec-WebSocket-Version: 13\r\n",
        ws_path, cli::g_cli_config.host.c_str(),
        cli::g_cli_config.port, key_b64);

    if (token && std::strlen(token) > 0) {
        len += std::snprintf(request + len,
            sizeof(request) - static_cast<size_t>(len),
            "Authorization: Bearer %s\r\n", token);
    }
    len += std::snprintf(request + len,
        sizeof(request) - static_cast<size_t>(len), "\r\n");

    if (cli::g_cli_config.verbose)
        std::println("[*] WS 握手请求:\n{}", request);

    /* 建立 TLS 连接 */
    void* ssl = nullptr;
    if (tls_client_init(nullptr) != 0) {
        std::println(stderr, "[-] TLS 初始化失败: {}", tls_last_error());
        return -1;
    }
    if (tls_client_connect(&ssl, cli::g_cli_config.host.c_str(),
            static_cast<unsigned short>(cli::g_cli_config.port)) != 0) {
        std::println(stderr, "[-] TLS 连接失败: {}", tls_last_error());
        return -1;
    }

    /* 发送握手请求 */
    int sent = 0;
    while (sent < len) {
        int n = static_cast<int>(tls_write(ssl, request + sent,
            static_cast<size_t>(len - sent)));
        if (n < 0) { tls_close(ssl); return -1; }
        sent += n;
    }

    /* 接收响应 */
    size_t total = 0;
    int n;
    while (total < sizeof(response) - 1) {
        n = static_cast<int>(tls_read(ssl, response + total,
            sizeof(response) - 1 - total));
        if (n < 0) { tls_close(ssl); return -1; }
        if (n == 0) break;
        total += static_cast<size_t>(n);
        if (std::strstr(response, "\r\n\r\n")) break;
    }
    response[total] = 0;

    if (std::strstr(response, "101") == nullptr) {
        std::println(stderr, "[-] WS 握手失败:\n{}", response);
        tls_close(ssl);
        return -1;
    }

    /* 验证 Accept Key */
    const char* accept_header = std::strstr(response, "Sec-WebSocket-Accept:");
    if (accept_header) {
        const char* val_start = accept_header + 20;
        while (*val_start == ' ') val_start++;
        char received_accept[64];
        int ai = 0;
        while (*val_start && *val_start != '\r' && *val_start != '\n' && ai < 60)
            received_accept[ai++] = *val_start++;
        received_accept[ai] = 0;
        if (std::strcmp(received_accept, accept_expected) != 0) {
            std::println("[-] WS Accept Key 不匹配!\n    期望: {}\n    收到: {}",
                         accept_expected, received_accept);
        } else {
            std::println("[+] WS Accept Key 验证通过");
        }
    }

    cli::g_cli_config.ws_ssl = ssl;
    cli::g_cli_config.ws_connected = true;
    cli::g_cli_config.ws_buffer_len = 0;
    std::println("[+] WebSocket 连接成功! ({}:{}{})",
                 cli::g_cli_config.host, cli::g_cli_config.port, ws_path);
    return 0;
}

static int ws_send_frame(int opcode, const unsigned char* payload,
                         size_t payload_len) noexcept {
    if (!cli::g_cli_config.ws_connected || !cli::g_cli_config.ws_ssl) {
        std::println(stderr, "[-] WebSocket 未连接");
        return -1;
    }

    unsigned char header[10];
    size_t header_len;
    header[0] = static_cast<unsigned char>(0x80 | (opcode & 0x0F));

    if (payload_len < 126) {
        header[1] = static_cast<unsigned char>(payload_len);
        header_len = 2;
    } else if (payload_len < 65536) {
        header[1] = 126;
        header[2] = static_cast<unsigned char>(payload_len >> 8);
        header[3] = static_cast<unsigned char>(payload_len);
        header_len = 4;
    } else {
        header[1] = 127;
        for (int i = 0; i < 8; i++)
            header[2 + i] = static_cast<unsigned char>(
                payload_len >> (56 - i * 8));
        header_len = 10;
    }

    int sent = 0;
    while (sent < static_cast<int>(header_len)) {
        int n = static_cast<int>(tls_write(cli::g_cli_config.ws_ssl,
            reinterpret_cast<const char*>(header + sent),
            header_len - static_cast<size_t>(sent)));
        if (n < 0) return -1;
        sent += n;
    }

    sent = 0;
    while (sent < static_cast<int>(payload_len)) {
        int n = static_cast<int>(tls_write(cli::g_cli_config.ws_ssl,
            reinterpret_cast<const char*>(payload + sent),
            payload_len - static_cast<size_t>(sent)));
        if (n < 0) return -1;
        sent += n;
    }
    return 0;
}

static int ws_recv_frame(int* out_opcode, unsigned char* out_payload,
                         size_t* out_len, size_t max_len) noexcept {
    if (!cli::g_cli_config.ws_connected || !cli::g_cli_config.ws_ssl)
        return -1;

    unsigned char header[2];
    int n = static_cast<int>(tls_read(cli::g_cli_config.ws_ssl,
        reinterpret_cast<char*>(header), 2));
    if (n < 0) {
        const char* err = tls_last_error();
        if (std::strstr(err, "timeout") || std::strstr(err, "again") ||
            std::strstr(err, "would block") || std::strstr(err, "AGAIN"))
            return 0;
        return -1;
    }
    if (n == 0) { cli::g_cli_config.ws_connected = false; return -1; }
    if (n < 2) return 0;

    int opcode = header[0] & 0x0F;
    bool masked = (header[1] & 0x80) != 0;
    uint64_t payload_len = header[1] & 0x7F;

    if (payload_len == 126) {
        unsigned char ext[2];
        if (tls_read(cli::g_cli_config.ws_ssl,
                reinterpret_cast<char*>(ext), 2) != 2) return -1;
        payload_len = (static_cast<uint64_t>(ext[0]) << 8) | ext[1];
    } else if (payload_len == 127) {
        unsigned char ext[8];
        if (tls_read(cli::g_cli_config.ws_ssl,
                reinterpret_cast<char*>(ext), 8) != 8) return -1;
        payload_len = 0;
        for (int i = 0; i < 8; i++)
            payload_len = (payload_len << 8) | ext[i];
    }

    unsigned char mask[4] = {0};
    if (masked && tls_read(cli::g_cli_config.ws_ssl,
            reinterpret_cast<char*>(mask), 4) != 4) return -1;

    if (payload_len > max_len) return -1;

    size_t read_total = 0;
    while (read_total < payload_len) {
        n = static_cast<int>(tls_read(cli::g_cli_config.ws_ssl,
            reinterpret_cast<char*>(out_payload + read_total),
            static_cast<size_t>(payload_len - read_total)));
        if (n <= 0) return -1;
        read_total += static_cast<size_t>(n);
    }

    if (masked)
        for (size_t i = 0; i < payload_len; i++)
            out_payload[i] ^= mask[i % 4];

    out_payload[payload_len] = 0;
    *out_opcode = opcode;
    *out_len = static_cast<size_t>(payload_len);
    return 1;
}

/** ws - WebSocket 调试命令 */
static int cmd_ws(int argc, char** argv) {
    if (argc < 1) {
        std::println("用法:");
        std::println("  ws connect <token> [path]    - 建立 WebSocket 连接");
        std::println("  ws send <json>               - 通过 WS 发送消息");
        std::println("  ws recv                      - 接收 WS 消息 (非阻塞)");
        std::println("  ws close                     - 关闭 WS 连接");
        std::println("  ws status                    - 查看 WS 连接状态");
        std::println("  ws monitor [rounds]          - WS 消息监控模式");
        return -1;
    }

    const char* subcmd = argv[0];

    if (std::strcmp(subcmd, "connect") == 0) {
        if (argc < 2) {
            std::println(stderr, "用法: ws connect <jwt_token> [path]");
            return -1;
        }
        return ws_connect(argv[1], (argc >= 3) ? argv[2] : "/api/ws");

    } else if (std::strcmp(subcmd, "send") == 0) {
        if (argc < 2) {
            std::println(stderr, "用法: ws send <json_data>");
            return -1;
        }
        const char* data = argv[1];
        std::println("[*] 发送 WS 消息: {}", data);
        int ret = ws_send_frame(WS_OPCODE_TEXT,
            reinterpret_cast<const unsigned char*>(data), std::strlen(data));
        if (ret == 0)
            std::println("[+] WS 消息发送成功 ({} 字节)", std::strlen(data));
        return ret;

    } else if (std::strcmp(subcmd, "recv") == 0) {
        unsigned char buf[16384];
        size_t rlen = 0;
        int opcode = 0;
        int ret = ws_recv_frame(&opcode, buf, &rlen, sizeof(buf) - 1);
        if (ret < 0) { std::println("[-] WS 接收失败"); return -1; }
        if (ret == 0) { std::println("[*] WS 无待接收消息"); return 0; }
        buf[rlen] = 0;
        if (opcode == WS_OPCODE_TEXT) {
            std::println("[*] WS 收到文本消息 ({} 字节):", rlen);
            print_json(reinterpret_cast<const char*>(buf), 4);
        } else if (opcode == WS_OPCODE_PING) {
            std::println("[*] WS Ping");
            ws_send_frame(WS_OPCODE_PONG, buf, rlen);
        } else if (opcode == WS_OPCODE_PONG) {
            std::println("[*] WS Pong");
        } else if (opcode == WS_OPCODE_CLOSE) {
            std::println("[*] WS 关闭帧");
            cli::g_cli_config.ws_connected = false;
        } else {
            std::println("[*] WS 帧 opcode=0x{:X} ({} 字节)", opcode, rlen);
        }
        return 0;

    } else if (std::strcmp(subcmd, "close") == 0) {
        if (cli::g_cli_config.ws_connected && cli::g_cli_config.ws_ssl) {
            unsigned char close_frame[2] = {0x88, 0x00};
            tls_write(cli::g_cli_config.ws_ssl,
                reinterpret_cast<const char*>(close_frame), 2);
            tls_close(cli::g_cli_config.ws_ssl);
        }
        cli::g_cli_config.ws_connected = false;
        cli::g_cli_config.ws_ssl = nullptr;
        std::println("[+] WebSocket 连接已关闭");
        return 0;

    } else if (std::strcmp(subcmd, "status") == 0) {
        std::println("[*] WebSocket 状态:");
        std::println("    连接: {}", cli::g_cli_config.ws_connected
                     ? "已连接" : "未连接");
        std::println("    服务器: {}:{}", cli::g_cli_config.host,
                     cli::g_cli_config.port);
        return 0;

    } else if (std::strcmp(subcmd, "monitor") == 0) {
        if (!cli::g_cli_config.ws_connected) {
            std::println(stderr, "[-] WebSocket 未连接");
            return -1;
        }
        int rounds = (argc >= 2) ? std::atoi(argv[1]) : 10;
        if (rounds < 1) rounds = 1;
        if (rounds > 100) rounds = 100;

        std::println("");
        std::println("  ╔══════════════════════════════════════════════════════════╗");
        std::println("  ║     WebSocket 监控模式 ({} 轮)                          ║", rounds);
        std::println("  ╚══════════════════════════════════════════════════════════╝");
        std::println("");

        int msg_count = 0;
        for (int r = 1; r <= rounds; r++) {
            unsigned char buf[16384];
            size_t rlen = 0;
            int opcode = 0;
            int ret = ws_recv_frame(&opcode, buf, &rlen, sizeof(buf) - 1);
            if (ret < 0) {
                std::println("  ⚠ 连接中断 (第 {} 轮)", r);
                break;
            }

            std::time_t now = std::time(nullptr);
            char ts[32];
            std::strftime(ts, sizeof(ts), "%H:%M:%S", std::localtime(&now));

            if (ret == 0) {
                std::println("  [{}] ♥ Ping (无消息)", ts);
                ws_send_frame(WS_OPCODE_PING, nullptr, 0);
            } else {
                msg_count++;
                buf[rlen] = 0;
                std::println("  ┌─────────────────────────────────────────────────────────┐");
                std::println("  │ [{}] WS 消息 #{}                                      │", ts, msg_count);
                std::println("  ├─────────────────────────────────────────────────────────┤");
                if (opcode == WS_OPCODE_TEXT)
                    std::println("  │ {}", reinterpret_cast<const char*>(buf));
                else
                    std::println("  │ ({} 字节数据)", rlen);
                std::println("  └─────────────────────────────────────────────────────────┘");
                if (opcode == WS_OPCODE_CLOSE) {
                    std::println("\n  [*] 对端发送关闭帧");
                    break;
                }
            }
            std::println("");
            if (r < rounds)
                std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        }
        std::println("  [*] 监控完成: 共 {} 条消息 ({} 轮)", msg_count, rounds);
        return 0;

    } else {
        std::println(stderr, "未知 ws 子命令: {}", subcmd);
        return -1;
    }
}

extern "C" void init_cmd_ws(void) {
    register_command("ws",
        "WebSocket 调试 (connect/send/recv/close/status/monitor)",
        "ws <connect|send|recv|close|status|monitor> ...",
        cmd_ws);
}
