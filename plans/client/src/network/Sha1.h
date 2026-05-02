/**
 * Chrono-shift 客户端 SHA-1 + Base64 工具
 * C++17 重构版
 */
#ifndef CHRONO_CLIENT_SHA1_H
#define CHRONO_CLIENT_SHA1_H

#include <cstdint>
#include <string>
#include <vector>

namespace chrono {
namespace client {
namespace network {

/**
 * SHA-1 哈希计算 + Base64 编码
 * 用于 WebSocket 握手验证 (Sec-WebSocket-Accept)
 */
class Sha1 {
public:
    Sha1();

    /** 重置状态 */
    void init();

    /** 更新数据 */
    void update(const uint8_t* data, size_t len);

    /** 完成计算，输出 20 字节摘要 */
    void final(uint8_t digest[20]);

    /**
     * Base64 编码
     * @param input 输入数据
     * @param input_len 输入长度
     * @param output 输出缓冲区 (应足够大)
     */
    static void base64_encode(const uint8_t* input, size_t input_len, char* output);

    /**
     * 计算 SHA-1 的便捷方法
     * @param data 输入数据
     * @return 20 字节摘要的 hex 字符串
     */
    static std::string hash(const std::string& data);

private:
    uint32_t state_[5];
    uint64_t count_;
    uint8_t buffer_[64];
};

} // namespace network
} // namespace client
} // namespace chrono

#endif // CHRONO_CLIENT_SHA1_H
