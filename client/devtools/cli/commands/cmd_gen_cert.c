/**
 * cmd_gen_cert.c — 快速生成自签名 TLS 证书命令
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
#include "../devtools_cli.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>   /* _mkdir */
#define mkdir(path, mode) _mkdir(path)
#else
#include <sys/stat.h> /* mkdir */
#endif

/* ============================================================
 * 常量
 * ============================================================ */

/** MSYS2 OpenSSL 路径 (Windows 优先使用) */
#define MSYS2_OPENSSL_PATH "D:\\mys32\\bin\\openssl.exe"

/** 默认证书输出目录 */
#define DEFAULT_OUTPUT_DIR "./certs"

/** 默认证书 Common Name */
#define DEFAULT_CN "127.0.0.1"

/** 默认证书有效天数 (10 年) */
#define DEFAULT_DAYS 3650

/** 子命令名称 */
#define CMD_NAME "gen-cert"

/* ============================================================
 * 辅助函数
 * ============================================================ */

/**
 * 检测 MSYS2 OpenSSL 是否可用
 * @return 1=可用, 0=不可用
 */
static int check_msys2_openssl(void)
{
#ifdef _WIN32
    FILE* f = fopen(MSYS2_OPENSSL_PATH, "rb");
    if (f) {
        fclose(f);
        return 1;
    }
#else
    (void)MSYS2_OPENSSL_PATH;
#endif
    return 0;
}

/**
 * 创建目录 (如果不存在)
 * @param path 目录路径
 * @return 0=成功或已存在, -1=失败
 */
static int ensure_dir(const char* path)
{
#ifdef _WIN32
    /* Windows: 检查目录是否存在 */
    DWORD attr = GetFileAttributesA(path);
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
        return 0; /* 已存在 */
    }
    /* 尝试创建 */
    if (mkdir(path) == 0) {
        return 0;
    }
    /* 如果失败但目录存在则也认为成功 */
    attr = GetFileAttributesA(path);
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
        return 0;
    }
    return -1;
#else
    struct stat st = {0};
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
        return 0; /* 已存在 */
    }
    return mkdir(path, 0755);
#endif
}

/**
 * 获取可用的 openssl 命令路径
 * @param buf  输出缓冲区
 * @param size 缓冲区大小
 * @return buf 指针
 */
static const char* get_openssl_cmd(char* buf, size_t size)
{
    /* 优先 MSYS2 */
#ifdef _WIN32
    if (check_msys2_openssl()) {
        snprintf(buf, size, "\"%s\"", MSYS2_OPENSSL_PATH);
        printf("[*] 使用 MSYS2 OpenSSL: %s\n", MSYS2_OPENSSL_PATH);
        return buf;
    }
#endif
    /* 回退到系统 PATH */
    snprintf(buf, size, "openssl");
    printf("[*] 使用系统 PATH 中的 openssl\n");
    return buf;
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
    if (argc > 3) days       = atoi(argv[3]);
    if (days <= 0) days = DEFAULT_DAYS;

    printf(COLOR_BOLD "=== 快速自签名证书生成 ===\n" COLOR_RESET);
    printf("  输出目录: %s\n", output_dir);
    printf("  Common Name: %s\n", cn);
    printf("  有效天数: %d\n", days);

    /* 确保输出目录存在 */
    if (ensure_dir(output_dir) != 0) {
        fprintf(stderr, COLOR_RED "[-] 无法创建输出目录: %s\n" COLOR_RESET, output_dir);
        return -1;
    }

    /* 构建输出文件路径 */
    char cert_path[512], key_path[512];
    snprintf(cert_path, sizeof(cert_path), "%s/cert.pem", output_dir);
    snprintf(key_path,  sizeof(key_path),  "%s/key.pem",  output_dir);

    /* 检查是否已存在 (避免误覆盖) */
    {
        FILE* f = fopen(cert_path, "rb");
        if (f) {
            fclose(f);
            printf(COLOR_YELLOW "[!] 证书文件已存在: %s\n" COLOR_RESET, cert_path);
            printf("    如需重新生成，请先删除该文件。\n");
            return -1;
        }
    }
    {
        FILE* f = fopen(key_path, "rb");
        if (f) {
            fclose(f);
            printf(COLOR_YELLOW "[!] 密钥文件已存在: %s\n" COLOR_RESET, key_path);
            printf("    如需重新生成，请先删除该文件。\n");
            return -1;
        }
    }

    /* 获取 openssl 命令 */
    char openssl_cmd[256];
    get_openssl_cmd(openssl_cmd, sizeof(openssl_cmd));

    /* ========================================================
     * 步骤 1: 生成 RSA 私钥
     * ======================================================== */
    printf("\n[1/3] 生成 RSA 2048 位私钥...\n");
    {
        char cmd[1024];
        snprintf(cmd, sizeof(cmd),
                 "%s genrsa -out \"%s\" 2048 2>&1",
                 openssl_cmd, key_path);

        printf("  执行: %s\n", cmd);
        int ret = system(cmd);
        if (ret != 0) {
            fprintf(stderr, COLOR_RED "[-] 私钥生成失败 (返回 %d)\n" COLOR_RESET, ret);
            return -1;
        }
    }

    /* ========================================================
     * 步骤 2: 生成自签名证书
     * ======================================================== */
    printf("[2/3] 生成自签名证书 (SHA-256)...\n");
    {
        char cmd[2048];
        snprintf(cmd, sizeof(cmd),
                 "%s req -x509 -new -nodes "
                 "-sha256 "
                 "-days %d "
                 "-key \"%s\" "
                 "-out \"%s\" "
                 "-subj \"/CN=%s/O=Chrono-Shift Dev/C=CN\" "
                 "-addext \"subjectAltName=DNS:%s,IP:127.0.0.1,IP:::1\" "
                 "2>&1",
                 openssl_cmd, days, key_path, cert_path, cn, cn);

        printf("  执行: %s\n", cmd);
        int ret = system(cmd);
        if (ret != 0) {
            fprintf(stderr, COLOR_RED "[-] 证书生成失败 (返回 %d)\n" COLOR_RESET, ret);
            /* 清理已生成的私钥 */
            remove(key_path);
            return -1;
        }
    }

    /* ========================================================
     * 步骤 3: 验证生成的证书
     * ======================================================== */
    printf("[3/3] 验证证书...\n");
    {
        char cmd[1024];
        snprintf(cmd, sizeof(cmd),
                 "%s x509 -in \"%s\" -text -noout 2>&1 | "
                 "%s findstr /C:\"Subject:\" /C:\"Not Before\" /C:\"Not After\"",
                 openssl_cmd, cert_path,
                 /* Windows 下使用 findstr, Linux 下使用 grep */
#ifdef _WIN32
                 ""
#else
                 "grep -E"
#endif
                );

        printf("  证书信息:\n");
        fflush(stdout);
        int ret = system(cmd);
        if (ret != 0) {
            /* 验证失败不是致命错误 */
            printf(COLOR_YELLOW "  [!] 证书验证输出异常 (非致命)\n" COLOR_RESET);
        }
    }

    /* ========================================================
     * 完成
     * ======================================================== */
    printf("\n" COLOR_GREEN "[+] 证书生成完成!\n" COLOR_RESET);
    printf("  " COLOR_CYAN "证书: %s\n" COLOR_RESET, cert_path);
    printf("  " COLOR_CYAN "私钥: %s\n" COLOR_RESET, key_path);
    printf("  " COLOR_CYAN "Common Name: %s\n" COLOR_RESET, cn);
    printf("  " COLOR_CYAN "有效期: %d 天\n" COLOR_RESET, days);
    printf("\n");
    printf("  使用方式:\n");
    printf("    ClientHttpServer::set_tls_cert_paths(\"%s\", \"%s\");\n",
           cert_path, key_path);
    printf("    ClientHttpServer::set_use_https(true);\n");
    printf("\n");
    printf("  " COLOR_YELLOW "注意: 此为自签名证书，仅用于开发/调试环境。\n" COLOR_RESET);
    printf("  " COLOR_YELLOW "      生产环境请使用受信任 CA 签发的证书。\n" COLOR_RESET);

    return 0;
}

/* ============================================================
 * 命令初始化入口
 * ============================================================ */

int init_cmd_gen_cert(void)
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
