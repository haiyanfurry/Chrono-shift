/**
 * Chrono-shift C++ 邮箱验证码发送器
 * SMTP 协议实现 (支持 SSL/TLS)
 * C++17 重构版 (P9.2)
 */
#ifndef CHRONO_CPP_EMAIL_VERIFIER_H
#define CHRONO_CPP_EMAIL_VERIFIER_H

#include <string>

namespace chrono {
namespace security {

/**
 * SMTP 配置
 */
struct SmtpConfig {
    std::string host = "smtp.qq.com";  // SMTP 服务器
    int port = 25;                      // 端口 (25=明文, 465=SSL, 587=TLS)
    std::string username;               // 邮箱账号
    std::string password;               // 邮箱密码/授权码
    std::string from_addr;              // 发件人地址 (通常同 username)
    std::string from_name = "墨竹";     // 发件人名称
    bool use_tls = false;               // 是否使用 TLS
};

/**
 * 邮箱验证码发送器
 * 通过 SMTP 协议发送验证码邮件
 */
class EmailVerifier {
public:
    explicit EmailVerifier(SmtpConfig config);

    /**
     * 发送验证码到指定邮箱
     * @param to_email 目标邮箱
     * @param code 6 位验证码
     * @return true 表示发送成功
     */
    bool send_code(const std::string& to_email, const std::string& code);

    /**
     * 获取当前配置 (只读)
     */
    const SmtpConfig& config() const { return config_; }

private:
    SmtpConfig config_;

    /**
     * 连接到 SMTP 服务器
     */
    bool connect_server(int& sock);

    /**
     * 发送 SMTP 命令并读取响应
     */
    bool send_command(int sock, const std::string& command,
                      std::string& response, bool wait_response = true);

    /**
     * Base64 编码
     */
    static std::string base64_encode(const std::string& input);

    /**
     * 构建验证码邮件内容
     */
    std::string build_email_body(const std::string& to_email,
                                  const std::string& code) const;
};

} // namespace security
} // namespace chrono

#endif // CHRONO_CPP_EMAIL_VERIFIER_H
