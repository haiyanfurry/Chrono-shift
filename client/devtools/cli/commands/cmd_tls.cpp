/**
 * cmd_tls.cpp — TLS 连接信息命令
 * 对应 debug_cli.c:1065 cmd_tls_info
 *
 * C++23 转换: std::println, namespace cli, extern "C"
 */
#include "../devtools_cli.hpp"

#include <print>     // std::println
#include <string>    // std::string

namespace cli = chrono::client::cli;

/* ============================================================
 * tls_client 函数 (由 core 或外部库提供)
 * ============================================================ */
extern "C" {
extern int tls_client_init(const char* cert_dir);
extern int tls_client_connect(void** ssl, const char* host, unsigned short port);
extern void tls_close(void* ssl);
extern const char* tls_last_error(void);
extern void tls_get_info(void* ssl, char* buf, size_t buf_size);
}

/* ============================================================
 * tls-info 命令 - 获取 TLS 连接信息
 * ============================================================ */
static int cmd_tls_info(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    std::println("[*] 获取 TLS 连接信息: {}:{}", cli::g_cli_config.host, cli::g_cli_config.port);

    /* 建立临时 TLS 连接来获取信息 */
    void* ssl = nullptr;
    if (tls_client_init(nullptr) != 0) {
        std::println(stderr, "[-] TLS 客户端初始化失败: {}", tls_last_error());
        return -1;
    }
    if (tls_client_connect(&ssl, cli::g_cli_config.host.c_str(),
                           static_cast<unsigned short>(cli::g_cli_config.port)) < 0) {
        std::println(stderr, "[-] 无法连接到 {}:{}: {}",
                     cli::g_cli_config.host, cli::g_cli_config.port, tls_last_error());
        return -1;
    }

    char info[2048]{};
    tls_get_info(ssl, info, sizeof(info));
    std::println("[+] TLS 连接信息:\n{}", info);

    tls_close(ssl);
    return 0;
}

extern "C" int init_cmd_tls_info(void)
{
    register_command("tls-info",
        "显示 TLS 连接信息",
        "tls-info",
        cmd_tls_info);
    return 0;
}
