/**
 * cmd_gen_cert.cpp — 快速生成自签名 TLS 证书命令
 *
 * C++23 转换: std::println, std::string, extern "C"
 *
 * 使用 OpenSSL 生成 RSA 2048 位自签名证书。
 * 优先使用 MSYS2 环境中的 openssl (D:\mys32\bin\openssl.exe)，
 * 其次使用系统 PATH 中的 openssl。
 *
 * 用法: gen-cert [output_dir] [cn] [days]
 *   output_dir  证书输出目录 (默认: ./certs)
 *   cn          证书 Common Name (默认: 127.0.0.1)
 *   days        证书有效天数 (默认: 3650, 即 10 年)
 *
 * 输出:
 *   output_dir/cert.pem  — 自签名证书
 *   output_dir/key.pem   — RSA 私钥
 */
#include "../devtools_cli.hpp"

#include <print>     // std::println
#include <string>    // std::string
#include <cstdio>    // std::snprintf, std::fopen, std::fclose, std::remove
#include <cstdlib>   // std::atoi, std::system

#ifdef _WIN32
#include <windows.h> /* GetFileAttributesA, CreateDirectoryA */
#else
#include <sys/stat.h> /* mkdir */
#endif

/* ============================================================
 * 常量
 * ============================================================ */

/** MSYS2 OpenSSL 路径 (Windows 优先使用) */
constexpr const char* MSYS2_OPENSSL_PATH = "D:\\mys32\\bin\\openssl.exe";

/** 默认证书输出目录 */
constexpr const char* DEFAULT_OUTPUT_DIR = "./certs";

/** 默认证书 Common Name */
constexpr const char* DEFAULT_CN = "127.0.0.1";

/** 默认证书有效天数 (10 年) */
constexpr int DEFAULT_DAYS = 3650;

/** 子命令名称 */
constexpr const char* CMD_NAME = "gen-cert";

/* ============================================================
 * 辅助函数
 * ============================================================ */

/**
 * 检测 MSYS2 OpenSSL 是否可用
 * @return true=可用, false=不可用
 */
static bool check_msys2_openssl() noexcept
{
#ifdef _WIN32
    FILE* f = std::fopen(MSYS2_OPENSSL_PATH, "rb");
    if (f) {
        std::fclose(f);
        return true;
    }
#endif
    return false;
}

/**
 * 创建目录 (如果不存在)
 * @param path 目录路径
 * @return true=成功或已存在, false=失败
 */
static bool ensure_dir(const std::string& path) noexcept
{
#ifdef _WIN32
    /* Windows: 检查目录是否存在 */
    DWORD attr = GetFileAttributesA(path.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
        return true; /* 已存在 */
    }
    /* 尝试创建 */
    if (CreateDirectoryA(path.c_str(), nullptr)) {
        return true;
    }
    /* 如果失败但目录存在则也认为成功 */
    attr = GetFileAttributesA(path.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
        return true;
    }
    return false;
#else
    struct stat st{};
    if (stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
        return true; /* 已存在 */
    }
    return mkdir(path.c_str(), 0755) == 0;
#endif
}

/**
 * 获取可用的 openssl 命令路径
 * @return openssl 命令字符串
 */
static std::string get_openssl_cmd() noexcept
{
    /* 优先 MSYS2 */
#ifdef _WIN32
    if (check_msys2_openssl()) {
        std::println("[*] 使用 MSYS2 OpenSSL: {}", MSYS2_OPENSSL_PATH);
        return std::string("\"") + MSYS2_OPENSSL_PATH + "\"";
    }
#endif
    /* 回退到系统 PATH */
    std::println("[*] 使用系统 PATH 中的 openssl");
    return "openssl";
}

/* ============================================================
 * gen-cert 命令主逻辑
 * ============================================================ */

static int cmd_gen_cert(int argc, char** argv)
{
    const char* output_dir = DEFAULT_OUTPUT_DIR;
    const char* cn         = DEFAULT_CN;
    int         days       = DEFAULT_DAYS;

    /* 解析参数 */
    if (argc > 1) output_dir = argv[1];
    if (argc > 2) cn         = argv[2];
    if (argc > 3) days       = std::atoi(argv[3]);
    if (days <= 0) days = DEFAULT_DAYS;

    std::println("{}=== 快速自签名证书生成 ==={}",
                 cli::COLOR_BOLD, cli::COLOR_RESET);
    std::println("  输出目录: {}", output_dir);
    std::println("  Common Name: {}", cn);
    std::println("  有效天数: {}", days);

    /* 确保输出目录存在 */
    if (!ensure_dir(output_dir)) {
        std::println(stderr, "{}[-] 无法创建输出目录: {}{}",
                     cli::COLOR_RED, output_dir, cli::COLOR_RESET);
        return -1;
    }

    /* 构建输出文件路径 */
    std::string cert_path = std::string(output_dir) + "/cert.pem";
    std::string key_path  = std::string(output_dir) + "/key.pem";

    /* 检查是否已存在 (避免误覆盖) */
    {
        FILE* f = std::fopen(cert_path.c_str(), "rb");
        if (f) {
            std::fclose(f);
            std::println("{}[!] 证书文件已存在: {}{}",
                         cli::COLOR_YELLOW, cert_path, cli::COLOR_RESET);
            std::println("    如需重新生成，请先删除该文件。");
            return -1;
        }
    }
    {
        FILE* f = std::fopen(key_path.c_str(), "rb");
        if (f) {
            std::fclose(f);
            std::println("{}[!] 密钥文件已存在: {}{}",
                         cli::COLOR_YELLOW, key_path, cli::COLOR_RESET);
            std::println("    如需重新生成，请先删除该文件。");
            return -1;
        }
    }

    /* 获取 openssl 命令 */
    std::string openssl_cmd = get_openssl_cmd();

    /* ========================================================
     * 步骤 1: 生成 RSA 私钥
     * ======================================================== */
    std::println("");
    std::println("[1/3] 生成 RSA 2048 位私钥...");
    {
        std::string cmd = openssl_cmd + " genrsa -out \"" + key_path + "\" 2048 2>&1";
        std::println("  执行: {}", cmd);
        int ret = std::system(cmd.c_str());
        if (ret != 0) {
            std::println(stderr, "{}[-] 私钥生成失败 (返回 {}){}",
                         cli::COLOR_RED, ret, cli::COLOR_RESET);
            return -1;
        }
    }

    /* ========================================================
     * 步骤 2: 生成自签名证书
     * ======================================================== */
    std::println("[2/3] 生成自签名证书 (SHA-256)...");
    {
        std::string cmd = openssl_cmd
            + " req -x509 -new -nodes "
            "-sha256 "
            "-days " + std::to_string(days) + " "
            "-key \"" + key_path + "\" "
            "-out \"" + cert_path + "\" "
            "-subj \"/CN=" + cn + "/O=Chrono-Shift Dev/C=CN\" "
            "-addext \"subjectAltName=DNS:" + cn + ",IP:127.0.0.1,IP:::1\" "
            "2>&1";

        std::println("  执行: {}", cmd);
        int ret = std::system(cmd.c_str());
        if (ret != 0) {
            std::println(stderr, "{}[-] 证书生成失败 (返回 {}){}",
                         cli::COLOR_RED, ret, cli::COLOR_RESET);
            /* 清理已生成的私钥 */
            std::remove(key_path.c_str());
            return -1;
        }
    }

    /* ========================================================
     * 步骤 3: 验证生成的证书
     * ======================================================== */
    std::println("[3/3] 验证证书...");
    {
        std::string cmd = openssl_cmd
            + " x509 -in \"" + cert_path + "\" -text -noout 2>&1 | "
#ifdef _WIN32
            "findstr /C:\"Subject:\" /C:\"Not Before\" /C:\"Not After\"";
#else
            "grep -E \"Subject:|Not Before|Not After\"";
#endif
        std::println("  证书信息:");
        std::fflush(stdout);
        int ret = std::system(cmd.c_str());
        if (ret != 0) {
            /* 验证失败不是致命错误 */
            std::println("{}  [!] 证书验证输出异常 (非致命){}",
                         cli::COLOR_YELLOW, cli::COLOR_RESET);
        }
    }

    /* ========================================================
     * 完成
     * ======================================================== */
    std::println("");
    std::println("{}[+] 证书生成完成!{}", cli::COLOR_GREEN, cli::COLOR_RESET);
    std::println("  {}证书: {}{}", cli::COLOR_CYAN, cert_path, cli::COLOR_RESET);
    std::println("  {}私钥: {}{}", cli::COLOR_CYAN, key_path, cli::COLOR_RESET);
    std::println("  {}Common Name: {}{}", cli::COLOR_CYAN, cn, cli::COLOR_RESET);
    std::println("  {}有效期: {} 天{}", cli::COLOR_CYAN, days, cli::COLOR_RESET);
    std::println("");
    std::println("  使用方式:");
    std::println("    ClientHttpServer::set_tls_cert_paths(\"{}\", \"{}\");",
                 cert_path, key_path);
    std::println("    ClientHttpServer::set_use_https(true);");
    std::println("");
    std::println("  {}注意: 此为自签名证书，仅用于开发/调试环境。{}",
                 cli::COLOR_YELLOW, cli::COLOR_RESET);
    std::println("  {}      生产环境请使用受信任 CA 签发的证书。{}",
                 cli::COLOR_YELLOW, cli::COLOR_RESET);

    return 0;
}

/* ============================================================
 * 命令初始化入口
 * ============================================================ */

extern "C" int init_cmd_gen_cert(void)
{
    register_command(CMD_NAME,
        "快速生成自签名 TLS 证书 (使用 OpenSSL)",
        "gen-cert [output_dir] [cn] [days]\n"
        "  output_dir   证书输出目录 (默认: ./certs)\n"
        "  cn           证书 Common Name (默认: 127.0.0.1)\n"
        "  days         证书有效天数 (默认: 3650)",
        cmd_gen_cert);
    return 0;
}
