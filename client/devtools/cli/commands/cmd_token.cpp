/**
 * cmd_token.cpp — JWT 令牌解码命令 (C++23 版本)
 */
#include "../devtools_cli.hpp"

#include <cctype>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <print>
#include <string>
#include <string_view>
#include <vector>

namespace cli = chrono::client::cli;

/** 解码 JWT 的单个部分 (Base64 -> JSON 打印) */
static void decode_jwt_part(std::string_view part)
{
    auto decoded = cli::base64_decode(part);
    if (!decoded) {
        std::println("        [解码失败 - 无效 Base64]");
        return;
    }
    // base64_decode 返回 vector<uint8_t>, 追加 null 终止符
    decoded->push_back(0);
    std::print("        ");
    cli::print_json(std::string_view(reinterpret_cast<const char*>(decoded->data()),
                                      decoded->size() - 1), 8);
}

/** token - 解码并分析 JWT */
static int cmd_token(int argc, char** argv)
{
    if (argc < 1) {
        std::println(stderr, "用法: token <jwt_token>");
        return -1;
    }

    std::string_view token = argv[0];
    std::println("[*] JWT 令牌分析");
    std::println("    令牌长度: {} 字符", token.size());
    std::print("    令牌前32位: ");
    for (std::size_t i = 0; i < 32 && i < token.size(); i++) {
        std::print("{}", token[i]);
    }
    if (token.size() > 32) std::print("...");
    std::println("\n");

    /* 按 '.' 分割 JWT */
    std::vector<std::string_view> parts;
    std::size_t start_pos = 0;
    for (int i = 0; i < 3; i++) {
        auto dot = token.find('.', start_pos);
        if (dot != std::string_view::npos && i < 2) {
            parts.push_back(token.substr(start_pos, dot - start_pos));
            start_pos = dot + 1;
        } else {
            parts.push_back(token.substr(start_pos));
            break;
        }
    }

    if (parts.size() < 2) {
        std::println("[-] 无效的 JWT 格式: 需要至少 2 个部分 (header.payload)");
        return -1;
    }

    /* 解码 Header */
    std::println("  [1] Header:");
    decode_jwt_part(parts[0]);

    /* 解码 Payload */
    std::println("  [2] Payload:");
    decode_jwt_part(parts[1]);

    /* 检查 Signature */
    if (parts.size() >= 3 && !parts[2].empty()) {
        std::println("  [3] Signature: {} 字节 (Base64 编码)", parts[2].size());
    } else {
        std::println("  [3] Signature: 无");
        std::println("[-] 警告: 令牌无签名, 可能被篡改!");
    }

    /* 从 payload 中提取过期时间 */
    auto payload_decoded = cli::base64_decode(parts[1]);
    if (payload_decoded) {
        payload_decoded->push_back(0);
        std::string_view payload_str(
            reinterpret_cast<const char*>(payload_decoded->data()),
            payload_decoded->size() - 1);

        /* 查找 exp 字段 */
        auto exp_pos = payload_str.find("\"exp\"");
        if (exp_pos != std::string_view::npos) {
            auto colon = payload_str.find(':', exp_pos);
            if (colon != std::string_view::npos) {
                long exp_val = std::strtol(payload_str.data() + colon + 1, nullptr, 10);
                if (exp_val > 0) {
                    std::time_t now = std::time(nullptr);
                    std::time_t exp_time = static_cast<std::time_t>(exp_val);
                    std::println("");
                    std::print("  [*] 过期时间: ");
                    // ctime 包含换行符
                    std::print("{}", std::ctime(&exp_time));
                    if (now >= exp_time) {
                        std::println("  [-] 令牌已过期!");
                    } else {
                        double remaining = std::difftime(exp_time, now);
                        std::println("  [+] 令牌有效, 剩余 {:.0f} 秒", remaining);
                    }
                }
            }
        }

        /* 查找 sub (user_id) 字段 */
        auto sub_pos = payload_str.find("\"sub\"");
        if (sub_pos != std::string_view::npos) {
            auto colon = payload_str.find(':', sub_pos);
            if (colon != std::string_view::npos) {
                auto val_start = colon + 1;
                while (val_start < payload_str.size() &&
                       std::isspace(static_cast<unsigned char>(payload_str[val_start]))) {
                    val_start++;
                }
                if (val_start < payload_str.size() && payload_str[val_start] == '"') {
                    val_start++;
                    auto val_end = payload_str.find('"', val_start);
                    if (val_end != std::string_view::npos) {
                        std::println("  [*] 用户 ID: {}",
                                     payload_str.substr(val_start, val_end - val_start));
                    }
                }
            }
        }
    }

    return 0;
}

extern "C" void init_cmd_token(void)
{
    register_command("token",
                     "解码并分析 JWT 令牌",
                     "token <jwt_token>",
                     cmd_token);
}
