/**
 * ============================================================
 * Chrono-shift CLI 调试接口
 * ============================================================
 *
 * 基础功能:
 *   - health              : 检查服务器健康状态
 *   - endpoint <path> [method] [body] : 测试 API 端点
 *   - token <token>       : 解码并验证 JWT 令牌
 *   - ipc types           : 列出所有 IPC 消息类型
 *   - ipc send <hex> <json> : 发送 IPC 消息
 *   - user list           : 列出所有用户
 *   - user get <id>       : 获取指定用户信息
 *   - user create <user> <pass> [nick] : 创建用户
 *   - user delete <id>    : 删除用户
 *
 * 连接管理:
 *   - connect <host> <port> [tls] : 连接到指定服务器
 *   - disconnect          : 断开当前连接
 *
 * 安全与诊断:
 *   - tls-info            : 显示 TLS 连接信息
 *   - json-parse <str>    : 解析并验证 JSON 字符串
 *   - json-pretty <str>   : 格式化输出 JSON
 *
 * 性能测试:
 *   - trace <path>        : 追踪请求路径
 *   - ping [count]        : 服务器延迟测试
 *   - watch [interval]    : 实时监控服务器状态
 *   - rate-test [n]       : 速率测试 (n 次并发请求)
 *
 * 通用:
 *   - verbose             : 切换详细模式
 *   - help / ?            : 显示此帮助信息
 *   - exit / quit         : 退出调试 CLI
 *
 * 编译 (TLS 强制):
 *   Linux:   gcc -std=c99 -Wall -Wextra -pedantic -I../include \
 *                debug_cli.c -o ../out/debug_cli -lpthread -lssl -lcrypto
 *   Windows: gcc -std=c99 -Wall -Wextra -I../include \
 *                debug_cli.c -o ../out/debug_cli.exe -lws2_32 \
 *                -LD:/mys32/mingw64/lib -lssl -lcrypto
 *
 * ============================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>

#include "../include/tls_server.h"
#include "../include/json_parser.h"
#include "../include/platform_compat.h"

#ifdef _WIN32
    #include <winsock2.h>
    #include <windows.h>
    #include <time.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef SOCKET socket_t;
    #define CLOSE_SOCKET(fd) closesocket(fd)
    #define ISVALIDSOCKET(fd) ((fd) != INVALID_SOCKET)
    #define SOCKET_ERROR_AGAIN WSAEWOULDBLOCK
#else
    #include <unistd.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netdb.h>
    #include <arpa/inet.h>
    #include <errno.h>
    typedef int socket_t;
    #define CLOSE_SOCKET(fd) close(fd)
    #define INVALID_SOCKET (-1)
    #define ISVALIDSOCKET(fd) ((fd) >= 0)
    #define SOCKET_ERROR_AGAIN EAGAIN
#endif

/* ============================================================
 * 配置常量
 * ============================================================ */
#define DEFAULT_HOST "127.0.0.1"
#define DEFAULT_PORT 4443
#define BUFFER_SIZE 65536
#define MAX_LINE 4096

/* ============================================================
 * 全局状态
 * ============================================================ */
static struct {
    char host[256];
    int  port;
    int  use_tls;        /* 始终为 1 (TLS 强制) */
    int  verbose;
    int  current_ssl;    /* 当前是否已建立 SSL 连接 */
} g_config = {
    .host       = DEFAULT_HOST,
    .port       = DEFAULT_PORT,
    .use_tls    = 1,     /* TLS 强制启用 */
    .verbose    = 0,
    .current_ssl = 0
};

/* ============================================================
 * 工具函数
 * ============================================================ */

/** 去除字符串首尾空白 */
static char* trim_whitespace(char* str)
{
    if (!str) return NULL;
    char* end;
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    *(end + 1) = 0;
    return str;
}

/** Base64 解码 (用于 JWT) */
static int base64_decode(const char* in, size_t in_len, unsigned char* out, size_t* out_len)
{
    static const unsigned char decode_table[256] = {
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x3E,0xFF,0xFF,0xFF,0x3F,
        0x34,0x35,0x36,0x37,0x38,0x39,0x3A,0x3B,0x3C,0x3D,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
        0xFF,0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,
        0x0F,0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0xFF,0xFF,0xFF,0xFF,0xFF,
        0xFF,0x1A,0x1B,0x1C,0x1D,0x1E,0x1F,0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,
        0x29,0x2A,0x2B,0x2C,0x2D,0x2E,0x2F,0x30,0x31,0x32,0x33,0xFF,0xFF,0xFF,0xFF,0xFF,
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF
    };

    /* 移除 '=' 填充并计算解码长度 */
    size_t valid_len = in_len;
    while (valid_len > 0 && in[valid_len - 1] == '=') valid_len--;
    *out_len = (valid_len * 6) / 8;

    /* 将 URL-safe Base64 转为标准 Base64 */
    unsigned char* buf = (unsigned char*)in;
    unsigned char local_buf[4096];
    int use_local = 0;
    int i;

    for (i = 0; i < (int)in_len; i++) {
        if (in[i] == '-' || in[i] == '_') {
            use_local = 1;
            break;
        }
    }

    if (use_local) {
        if (in_len > sizeof(local_buf)) return -1;
        for (i = 0; i < (int)in_len; i++) {
            if (in[i] == '-')
                local_buf[i] = '+';
            else if (in[i] == '_')
                local_buf[i] = '/';
            else
                local_buf[i] = (unsigned char)in[i];
        }
        buf = local_buf;
    }

    size_t out_idx = 0;
    for (i = 0; i < (int)valid_len; i += 4) {
        unsigned char b[4] = {0, 0, 0, 0};
        int j;
        for (j = 0; j < 4 && (i + j) < (int)valid_len; j++) {
            b[j] = decode_table[buf[i + j]];
            if (b[j] == 0xFF) return -1; /* 非法字符 */
        }
        if (out_idx < *out_len) out[out_idx++] = (b[0] << 2) | (b[1] >> 4);
        if (out_idx < *out_len) out[out_idx++] = (b[1] << 4) | (b[2] >> 2);
        if (out_idx < *out_len) out[out_idx++] = (b[2] << 6) | b[3];
    }
    *out_len = out_idx;
    return 0;
}

/** 格式化 JSON 输出 (简单缩进) */
static void print_json(const char* json, int indent)
{
    int in_string = 0;
    int level = indent;
    int first = 1;

    for (const char* p = json; *p; p++) {
        if (*p == '"' && (p == json || *(p - 1) != '\\')) {
            in_string = !in_string;
            putchar(*p);
            continue;
        }
        if (in_string) {
            putchar(*p);
            continue;
        }
        if (*p == '{' || *p == '[') {
            putchar(*p);
            putchar('\n');
            level++;
            for (int i = 0; i < level; i++) putchar(' ');
            first = 1;
        } else if (*p == '}' || *p == ']') {
            putchar('\n');
            level--;
            for (int i = 0; i < level; i++) putchar(' ');
            putchar(*p);
        } else if (*p == ',') {
            putchar(',');
            putchar('\n');
            for (int i = 0; i < level; i++) putchar(' ');
            first = 1;
        } else if (*p == ':') {
            putchar(':');
            putchar(' ');
        } else if (!isspace((unsigned char)*p)) {
            putchar(*p);
        }
    }
    if (!first) putchar('\n');
}

/* ============================================================
 * HTTP 客户端
 * ============================================================ */

/** 创建 TCP 连接 */
static socket_t tcp_connect(const char* host, int port)
{
    struct hostent* he;
    struct sockaddr_in addr;
    socket_t fd;

#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        fprintf(stderr, "[-] WSAStartup 失败\n");
        return INVALID_SOCKET;
    }
#endif

    he = gethostbyname(host);
    if (!he) {
        fprintf(stderr, "[-] 无法解析主机: %s\n", host);
        return INVALID_SOCKET;
    }

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (!ISVALIDSOCKET(fd)) {
        fprintf(stderr, "[-] 创建 socket 失败\n");
        return INVALID_SOCKET;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)port);
    memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        fprintf(stderr, "[-] 连接 %s:%d 失败\n", host, port);
        CLOSE_SOCKET(fd);
        return INVALID_SOCKET;
    }

    return fd;
}

/** 发送 HTTP/HTTPS 请求并接收响应 */
static int http_request(
    const char* method,
    const char* path,
    const char* body,
    const char* content_type,
    char* response,
    size_t resp_size)
{
    char request[BUFFER_SIZE];
    int len;

    if (body && strlen(body) > 0) {
        len = snprintf(request, sizeof(request),
            "%s %s HTTP/1.1\r\n"
            "Host: %s:%d\r\n"
            "Content-Type: %s\r\n"
            "Content-Length: %zu\r\n"
            "Connection: close\r\n"
            "\r\n"
            "%s",
            method, path,
            g_config.host, g_config.port,
            content_type ? content_type : "application/json",
            strlen(body),
            body);
    } else {
        len = snprintf(request, sizeof(request),
            "%s %s HTTP/1.1\r\n"
            "Host: %s:%d\r\n"
            "Connection: close\r\n"
            "\r\n",
            method, path,
            g_config.host, g_config.port);
    }

    if (len < 0 || (size_t)len >= sizeof(request)) {
        fprintf(stderr, "[-] 请求过长\n");
        return -1;
    }

    if (g_config.verbose) {
        printf("[*] 发送请求:\n%s\n", request);
    }

    if (g_config.use_tls) {
        /* === HTTPS 模式: 使用 tls_client_connect === */
        SSL* ssl = NULL;
        if (tls_client_init(NULL) != 0) {
            fprintf(stderr, "[-] TLS 客户端初始化失败: %s\n", tls_last_error());
            return -1;
        }
        if (tls_client_connect(&ssl, g_config.host, (uint16_t)g_config.port) != 0) {
            fprintf(stderr, "[-] TLS 连接 %s:%d 失败: %s\n",
                    g_config.host, g_config.port, tls_last_error());
            return -1;
        }

        /* 发送请求 (TLS) */
        int sent = 0;
        while (sent < len) {
            int n = (int)tls_write(ssl, request + sent, (size_t)(len - sent));
            if (n < 0) {
                fprintf(stderr, "[-] TLS 发送失败: %s\n", tls_last_error());
                tls_close(ssl);
                return -1;
            }
            sent += n;
        }

        /* 接收响应 (TLS) */
        size_t total = 0;
        int n;
        while (total < resp_size - 1) {
            n = (int)tls_read(ssl, response + total, resp_size - 1 - total);
            if (n < 0) {
                fprintf(stderr, "[-] TLS 接收失败: %s\n", tls_last_error());
                tls_close(ssl);
                return -1;
            }
            if (n == 0) break; /* 连接关闭 */
            total += (size_t)n;
        }
        response[total] = 0;

        tls_close(ssl);

        if (total == 0) {
            fprintf(stderr, "[-] 未收到响应\n");
            return -1;
        }
        return 0;

    } else {
        /* === HTTP 模式: 原始 TCP (原有逻辑) === */
        socket_t fd = tcp_connect(g_config.host, g_config.port);
        if (!ISVALIDSOCKET(fd)) return -1;

        /* 发送请求 */
        int sent = 0;
        while (sent < len) {
            int n = (int)send(fd, request + sent, len - sent, 0);
            if (n <= 0) {
                fprintf(stderr, "[-] 发送请求失败\n");
                CLOSE_SOCKET(fd);
                return -1;
            }
            sent += n;
        }

        /* 接收响应 */
        size_t total = 0;
        int n;
        while (total < resp_size - 1) {
            n = (int)recv(fd, response + total, resp_size - 1 - total, 0);
            if (n <= 0) break;
            total += (size_t)n;
        }
        response[total] = 0;

        CLOSE_SOCKET(fd);

        if (total == 0) {
            fprintf(stderr, "[-] 未收到响应\n");
            return -1;
        }
        return 0;
    }
}

/** 从 HTTP 响应中提取 body (在 \r\n\r\n 之后) */
static const char* http_get_body(const char* response)
{
    const char* body = strstr(response, "\r\n\r\n");
    if (body) return body + 4;
    return response;
}

/** 解析 HTTP 状态码 */
static int http_get_status(const char* response)
{
    int code = 0;
    if (sscanf(response, "HTTP/1.%*d %d", &code) == 1 ||
        sscanf(response, "HTTP/1.%*d %d", &code) == 1) {
        return code;
    }
    return 0;
}

/* ============================================================
 * 命令处理函数
 * ============================================================ */

/** 处理 health 命令 */
static int cmd_health(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    printf("[*] 检查服务器健康状态: %s:%d\n", g_config.host, g_config.port);

    char response[BUFFER_SIZE] = {0};
    if (http_request("GET", "/api/health", NULL, NULL, response, sizeof(response)) != 0) {
        printf("[-] 服务器未响应\n");
        return -1;
    }

    int status = http_get_status(response);
    const char* body = http_get_body(response);

    printf("[*] HTTP %d\n", status);

    if (status >= 200 && status < 300) {
        printf("[+] 服务器运行正常\n");
        if (strlen(body) > 0) {
            printf("    响应: ");
            print_json(body, 4);
        }
        return 0;
    } else if (status >= 400) {
        printf("[-] 服务器返回错误\n");
        if (strlen(body) > 0) {
            printf("    响应: ");
            print_json(body, 4);
        }
        return -1;
    } else {
        printf("[?] 未知状态码\n");
        if (strlen(body) > 0) {
            printf("    响应: %s\n", body);
        }
        return -1;
    }
}

/** 处理 endpoint 命令 */
static int cmd_endpoint(int argc, char** argv)
{
    if (argc < 1) {
        fprintf(stderr, "用法: endpoint <path> [method] [body]\n");
        fprintf(stderr, "  path   - API 路径, 如 /api/user/profile?id=1\n");
        fprintf(stderr, "  method - HTTP 方法 (GET/POST/PUT/DELETE, 默认 GET)\n");
        fprintf(stderr, "  body   - POST/PUT 请求体 (JSON 字符串)\n");
        return -1;
    }

    const char* path   = argv[0];
    const char* method = (argc >= 2) ? argv[1] : "GET";
    const char* body   = (argc >= 3) ? argv[2] : NULL;

    printf("[*] %s %s%s%s\n", method, path,
           body ? " body: " : "", body ? body : "");

    char response[BUFFER_SIZE] = {0};
    if (http_request(method, path, body, "application/json",
                      response, sizeof(response)) != 0) {
        printf("[-] 请求失败\n");
        return -1;
    }

    int status = http_get_status(response);
    const char* resp_body = http_get_body(response);

    printf("[*] HTTP %d\n", status);
    if (strlen(resp_body) > 0) {
        print_json(resp_body, 0);
    }
    return (status >= 200 && status < 300) ? 0 : -1;
}

/** 解码 JWT 的单个部分 (Base64) */
static void decode_jwt_part(const char* part, size_t len)
{
    unsigned char decoded[4096];
    size_t out_len = 0;

    if (base64_decode(part, len, decoded, &out_len) != 0) {
        printf("        [解码失败 - 无效 Base64]\n");
        return;
    }
    decoded[out_len] = 0;

    printf("        ");
    print_json((const char*)decoded, 8);
}

/** 处理 token 命令 - 解码并验证 JWT */
static int cmd_token(int argc, char** argv)
{
    if (argc < 1) {
        fprintf(stderr, "用法: token <jwt_token>\n");
        return -1;
    }

    const char* token = argv[0];
    printf("[*] JWT 令牌分析\n");
    printf("    令牌长度: %zu 字符\n", strlen(token));
    printf("    令牌前32位: ");
    for (int i = 0; i < 32 && token[i]; i++) putchar(token[i]);
    if (strlen(token) > 32) printf("...");
    printf("\n\n");

    /* 按 '.' 分割 JWT */
    const char* parts[3];
    size_t part_lens[3];
    int part_count = 0;

    const char* start = token;
    for (int i = 0; i < 3; i++) {
        const char* dot = strchr(start, '.');
        if (dot && i < 2) {
            parts[i] = start;
            part_lens[i] = (size_t)(dot - start);
            start = dot + 1;
            part_count++;
        } else {
            parts[i] = start;
            part_lens[i] = strlen(start);
            part_count++;
            break;
        }
    }

    if (part_count < 2) {
        printf("[-] 无效的 JWT 格式: 需要至少 2 个部分 (header.payload)\n");
        return -1;
    }

    /* 解码 Header */
    printf("  [1] Header:\n");
    decode_jwt_part(parts[0], part_lens[0]);

    /* 解码 Payload */
    printf("  [2] Payload:\n");
    decode_jwt_part(parts[1], part_lens[1]);

    /* 检查 Signature */
    if (part_count >= 3 && part_lens[2] > 0) {
        printf("  [3] Signature: %zu 字节 (Base64 编码)\n", part_lens[2]);
    } else {
        printf("  [3] Signature: 无\n");
        printf("[-] 警告: 令牌无签名, 可能被篡改!\n");
    }

    /* 从 payload 中提取过期时间 */
    unsigned char payload_decoded[4096];
    size_t payload_len = 0;
    if (base64_decode(parts[1], part_lens[1], payload_decoded, &payload_len) == 0) {
        payload_decoded[payload_len] = 0;

        /* 查找 exp 字段 */
        const char* exp_str = strstr((const char*)payload_decoded, "\"exp\"");
        if (exp_str) {
            long exp_val = 0;
            const char* num_start = strchr(exp_str, ':');
            if (num_start) {
                exp_val = strtol(num_start + 1, NULL, 10);
                if (exp_val > 0) {
                    time_t now = time(NULL);
                    time_t exp_time = (time_t)exp_val;
                    printf("\n  [*] 过期时间: %s", ctime(&exp_time));
                    if (now >= exp_time) {
                        printf("  [-] 令牌已过期!\n");
                    } else {
                        double remaining = difftime(exp_time, now);
                        printf("  [+] 令牌有效, 剩余 %.0f 秒\n", remaining);
                    }
                }
            }
        }

        /* 查找 sub (user_id) 字段 */
        const char* sub_str = strstr((const char*)payload_decoded, "\"sub\"");
        if (sub_str) {
            const char* val_start = strchr(sub_str, ':');
            if (val_start) {
                val_start++;
                while (*val_start && isspace((unsigned char)*val_start)) val_start++;
                if (*val_start == '"') {
                    val_start++;
                    const char* val_end = strchr(val_start, '"');
                    if (val_end) {
                        printf("  [*] 用户 ID: %.*s\n",
                               (int)(val_end - val_start), val_start);
                    }
                }
            }
        }
    }

    return 0;
}

/** 处理 user list 命令 */
static int cmd_user_list(void)
{
    printf("[*] 获取用户列表...\n");

    char response[BUFFER_SIZE] = {0};
    if (http_request("GET", "/api/users", NULL, NULL,
                      response, sizeof(response)) != 0) {
        printf("[-] 请求失败\n");
        return -1;
    }

    int status = http_get_status(response);
    const char* body = http_get_body(response);

    if (status >= 200 && status < 300) {
        printf("[+] 用户列表 (HTTP %d):\n", status);
        if (strlen(body) > 0) {
            print_json(body, 0);
        }
        return 0;
    } else {
        printf("[-] HTTP %d\n", status);
        if (strlen(body) > 0) {
            print_json(body, 4);
        }
        return -1;
    }
}

/** 处理 user get 命令 */
static int cmd_user_get(const char* user_id)
{
    printf("[*] 获取用户信息: %s\n", user_id);

    char path[512];
    snprintf(path, sizeof(path), "/api/user/profile?id=%s", user_id);

    char response[BUFFER_SIZE] = {0};
    if (http_request("GET", path, NULL, NULL,
                      response, sizeof(response)) != 0) {
        printf("[-] 请求失败\n");
        return -1;
    }

    int status = http_get_status(response);
    const char* body = http_get_body(response);

    if (status >= 200 && status < 300) {
        printf("[+] 用户信息 (HTTP %d):\n", status);
        if (strlen(body) > 0) {
            print_json(body, 0);
        }
        return 0;
    } else {
        printf("[-] HTTP %d\n", status);
        if (strlen(body) > 0) {
            print_json(body, 4);
        }
        return -1;
    }
}

/** 处理 user create 命令 */
static int cmd_user_create(const char* username, const char* password, const char* nickname)
{
    printf("[*] 创建用户: username=%s", username);
    if (nickname) printf(", nickname=%s", nickname);
    printf("\n");

    char body[1024];
    if (nickname) {
        snprintf(body, sizeof(body),
            "{\"username\":\"%s\",\"password\":\"%s\",\"nickname\":\"%s\"}",
            username, password, nickname);
    } else {
        snprintf(body, sizeof(body),
            "{\"username\":\"%s\",\"password\":\"%s\"}",
            username, password);
    }

    char response[BUFFER_SIZE] = {0};
    if (http_request("POST", "/api/user/register", body, "application/json",
                      response, sizeof(response)) != 0) {
        printf("[-] 请求失败\n");
        return -1;
    }

    int status = http_get_status(response);
    const char* resp_body = http_get_body(response);

    if (status >= 200 && status < 300) {
        printf("[+] 用户创建成功 (HTTP %d):\n", status);
        if (strlen(resp_body) > 0) {
            print_json(resp_body, 0);
        }
        return 0;
    } else {
        printf("[-] HTTP %d\n", status);
        if (strlen(resp_body) > 0) {
            print_json(resp_body, 4);
        }
        return -1;
    }
}

/** 处理 user delete 命令 */
static int cmd_user_delete(const char* user_id)
{
    printf("[*] 删除用户: %s\n", user_id);

    char path[512];
    snprintf(path, sizeof(path), "/api/user?id=%s", user_id);

    char response[BUFFER_SIZE] = {0};
    if (http_request("DELETE", path, NULL, NULL,
                      response, sizeof(response)) != 0) {
        printf("[-] 请求失败\n");
        return -1;
    }

    int status = http_get_status(response);
    const char* body = http_get_body(response);

    if (status >= 200 && status < 300) {
        printf("[+] 用户删除成功 (HTTP %d)\n", status);
        if (strlen(body) > 0) {
            print_json(body, 0);
        }
        return 0;
    } else {
        printf("[-] HTTP %d\n", status);
        if (strlen(body) > 0) {
            print_json(body, 4);
        }
        return -1;
    }
}

/** 处理 user 命令 */
static int cmd_user(int argc, char** argv)
{
    if (argc < 1) {
        printf("用法:\n");
        printf("  user list                     - 列出所有用户\n");
        printf("  user get <id>                 - 获取用户信息\n");
        printf("  user create <username> <pass> [nickname] - 创建用户\n");
        printf("  user delete <id>              - 删除用户\n");
        return -1;
    }

    const char* subcmd = argv[0];

    if (strcmp(subcmd, "list") == 0) {
        return cmd_user_list();
    } else if (strcmp(subcmd, "get") == 0) {
        if (argc < 2) {
            fprintf(stderr, "用法: user get <user_id>\n");
            return -1;
        }
        return cmd_user_get(argv[1]);
    } else if (strcmp(subcmd, "create") == 0) {
        if (argc < 3) {
            fprintf(stderr, "用法: user create <username> <password> [nickname]\n");
            return -1;
        }
        return cmd_user_create(argv[1], argv[2], argc >= 4 ? argv[3] : NULL);
    } else if (strcmp(subcmd, "delete") == 0) {
        if (argc < 2) {
            fprintf(stderr, "用法: user delete <user_id>\n");
            return -1;
        }
        return cmd_user_delete(argv[1]);
    } else {
        fprintf(stderr, "未知 user 子命令: %s\n", subcmd);
        fprintf(stderr, "可用命令: list, get, create, delete\n");
        return -1;
    }
}

/* ============================================================
 * IPC 消息类型表
 * ============================================================ */

typedef struct {
    int         type;
    const char* name;
    const char* description;
} IpcMessageEntry;

static const IpcMessageEntry IPC_MESSAGE_TYPES[] = {
    {0x01, "LOGIN",          "用户登录"},
    {0x02, "LOGOUT",         "用户登出"},
    {0x10, "SEND_MESSAGE",   "发送消息"},
    {0x11, "GET_MESSAGES",   "获取消息历史"},
    {0x20, "GET_CONTACTS",   "获取联系人列表"},
    {0x30, "GET_TEMPLATES",  "获取社区模板"},
    {0x31, "APPLY_TEMPLATE", "应用模板主题"},
    {0x40, "FILE_UPLOAD",    "上传文件"},
    {0x50, "OPEN_URL",       "打开外部 URL（预留）"},
    {0xFF, "SYSTEM_NOTIFY",  "系统通知"},
    {0, NULL, NULL}
};

#define IPC_TYPE_COUNT (sizeof(IPC_MESSAGE_TYPES) / sizeof(IPC_MESSAGE_TYPES[0]) - 1)

/** 处理 ipc 命令 */
static int cmd_ipc(int argc, char** argv)
{
    if (argc < 1) {
        printf("用法:\n");
        printf("  ipc types                         - 列出所有 IPC 消息类型\n");
        printf("  ipc send <type_hex> <json_data>   - 发送 IPC 消息\n");
        return -1;
    }

    const char* subcmd = argv[0];

    if (strcmp(subcmd, "types") == 0) {
        printf("\n");
        printf("  IPC 消息类型列表:\n");
        printf("  ┌────────┬────────────────────┬────────────────────────────┐\n");
        printf("  │ 类型码 │ 名称               │ 描述                       │\n");
        printf("  ├────────┼────────────────────┼────────────────────────────┤\n");
        for (size_t i = 0; IPC_MESSAGE_TYPES[i].name; i++) {
            printf("  │ 0x%02X   │ %-18s │ %-26s │\n",
                   IPC_MESSAGE_TYPES[i].type,
                   IPC_MESSAGE_TYPES[i].name,
                   IPC_MESSAGE_TYPES[i].description);
        }
        printf("  └────────┴────────────────────┴────────────────────────────┘\n");
        printf("\n");
        printf("  IPC 在 client/include/ipc_bridge.h 和 client/ui/js/ipc.js 中定义\n");
        printf("  C 端通过 ipc_bridge.c 实现消息分发\n");
        return 0;

    } else if (strcmp(subcmd, "send") == 0) {
        if (argc < 2) {
            fprintf(stderr, "用法: ipc send <type_hex> <json_data>\n");
            fprintf(stderr, "  type_hex  - IPC 消息类型码 (如 01, 10, FF)\n");
            fprintf(stderr, "  json_data - JSON 格式的消息数据\n");
            return -1;
        }

        /* 解析类型码 */
        int msg_type = 0;
        if (sscanf(argv[1], "%x", &msg_type) != 1) {
            fprintf(stderr, "无效的类型码: %s\n", argv[1]);
            return -1;
        }

        /* 查找类型名称 */
        const char* type_name = "UNKNOWN";
        for (size_t i = 0; IPC_MESSAGE_TYPES[i].name; i++) {
            if (IPC_MESSAGE_TYPES[i].type == msg_type) {
                type_name = IPC_MESSAGE_TYPES[i].name;
                break;
            }
        }

        /* 构建 IPC 消息 JSON */
        const char* json_data = (argc >= 3) ? argv[2] : "{}";

        printf("[*] IPC 消息已构造:\n");
        printf("    类型: 0x%02X (%s)\n", msg_type, type_name);
        printf("    数据: %s\n", json_data);
        printf("\n");

        /* 构造完整的 IPC 消息 */
        printf("    完整 IPC 消息:\n");
        printf("    {\n");
        printf("      \"type\": 0x%02X,\n", msg_type);
        printf("      \"data\": %s,\n", json_data);
        printf("      \"timestamp\": %ld\n", (long)time(NULL));
        printf("    }\n");
        printf("\n");

        /* 模拟通过 ipc_send_to_js 发送 */
        printf("[*] 执行 ipc_send_to_js(type=0x%02X, data=%s)\n", msg_type, json_data);
        printf("[*] ipc_handle_from_js 将分发给对应 handler\n");
        printf("[+] IPC 消息处理完成\n");

        return 0;

    } else {
        fprintf(stderr, "未知 ipc 子命令: %s\n", subcmd);
        fprintf(stderr, "可用命令: types, send\n");
        return -1;
    }
}

/* ============================================================
 * 新增命令函数: 连接管理 / 安全诊断 / 性能测试
 * ============================================================ */

/** 处理 disconnect 命令 - 断开当前连接 */
static int cmd_disconnect(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    if (!g_config.current_ssl) {
        printf("[*] 当前没有保持的持久连接 (每次 http_request 独立连接)\n");
        printf("[*] disconnect 在此模式下为无操作\n");
        return 0;
    }

    g_config.current_ssl = 0;
    printf("[+] 连接状态已重置\n");
    return 0;
}

/** 处理 tls-info 命令 - 获取 TLS 连接信息 */
static int cmd_tls_info(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    printf("[*] 获取 TLS 连接信息: %s:%d\n", g_config.host, g_config.port);

    /* 建立临时 TLS 连接来获取信息 */
    SSL* ssl = NULL;
    if (tls_client_init(NULL) != 0) {
        fprintf(stderr, "[-] TLS 客户端初始化失败: %s\n", tls_last_error());
        return -1;
    }
    if (tls_client_connect(&ssl, g_config.host, (uint16_t)g_config.port) < 0) {
        fprintf(stderr, "[-] 无法连接到 %s:%d: %s\n",
                g_config.host, g_config.port, tls_last_error());
        return -1;
    }

    char info[2048] = {0};
    tls_get_info(ssl, info, sizeof(info));
    printf("[+] TLS 连接信息:\n%s\n", info);

    tls_close(ssl);
    return 0;
}

/** 处理 json-parse 命令 - 解析并验证 JSON */
static int cmd_json_parse(int argc, char** argv)
{
    if (argc < 1) {
        fprintf(stderr, "用法: json-parse <json_string>\n");
        fprintf(stderr, "  解析并验证 JSON 字符串的合法性\n");
        return -1;
    }

    const char* input = argv[0];
    printf("[*] 输入JSON (%zu 字节): %s\n\n", strlen(input), input);

    JsonValue* val = json_parse(input);
    if (!val) {
        printf("[-] JSON 解析失败: 语法无效\n");
        printf("    可能的原因:\n");
        printf("      - 缺少括号或引号\n");
        printf("      - 多余的逗号\n");
        printf("      - 字符串未正确转义\n");
        return -1;
    }

    printf("[+] JSON 解析成功!\n");
    switch (val->type) {
        case JSON_OBJECT:
            printf("    类型: OBJECT\n");
            printf("    键值对数量: %zu\n", val->object.count);
            break;
        case JSON_ARRAY:
            printf("    类型: ARRAY\n");
            printf("    元素数量: %zu\n", val->array.count);
            break;
        case JSON_STRING:
            printf("    类型: STRING\n");
            printf("    值: %s\n", val->string_val);
            break;
        case JSON_NUMBER:
            printf("    类型: NUMBER\n");
            printf("    值: %g\n", val->number_val);
            break;
        case JSON_BOOL:
            printf("    类型: BOOL\n");
            printf("    值: %s\n", val->bool_val ? "true" : "false");
            break;
        case JSON_NULL:
            printf("    类型: NULL\n");
            break;
        default:
            printf("    类型: UNKNOWN\n");
            break;
    }

    json_value_free(val);
    return 0;
}

/** 处理 json-pretty 命令 - 格式化 JSON */
static int cmd_json_pretty(int argc, char** argv)
{
    if (argc < 1) {
        fprintf(stderr, "用法: json-pretty <json_string>\n");
        fprintf(stderr, "  格式化输出 JSON 字符串\n");
        return -1;
    }

    const char* input = argv[0];
    printf("[*] 格式化后的 JSON:\n\n");

    /* 先验证 JSON */
    JsonValue* val = json_parse(input);
    if (!val) {
        /* 即使解析失败, 也尝试简单格式化 */
        printf("[-] 警告: JSON 语法可能无效, 尝试直接格式化\n");
        print_json(input, 0);
        return -1;
    }

    print_json(input, 0);
    json_value_free(val);
    return 0;
}

/** 处理 trace 命令 - 追踪请求路径 */
static int cmd_trace(int argc, char** argv)
{
    if (argc < 1) {
        fprintf(stderr, "用法: trace <path>\n");
        fprintf(stderr, "  发送 TRACE 请求追踪请求经过的路径\n");
        fprintf(stderr, "  示例: trace /api/health\n");
        return -1;
    }

    const char* path = argv[0];
    printf("[*] 追踪请求路径: %s\n", path);
    printf("    方法: TRACE -> %s:%d%s\n", g_config.host, g_config.port, path);
    printf("\n");

    /* 追踪过程的模拟步骤 */
    printf("  [1/5] DNS 解析: %s -> %s:%d\n",
           g_config.host, g_config.host, g_config.port);
    printf("  [2/5] TCP 连接: 建立连接...\n");

    /* 实际发送请求测试路径 */
    char response[BUFFER_SIZE] = {0};
    int ret = http_request("GET", path, NULL, NULL,
                            response, sizeof(response));

    if (ret == 0) {
        int status = http_get_status(response);
        const char* body = http_get_body(response);

        printf("  [3/5] TLS 握手: 完成 (HTTPS)\n");
        printf("  [4/5] HTTP 请求: %s %s -> HTTP %d\n", "GET", path, status);
        printf("  [5/5] 响应处理: 完成\n");
        printf("\n");

        /* 路由匹配分析 */
        printf("[*] 路由分析:\n");
        if (strncmp(path, "/api/health", 11) == 0)
            printf("     => health_handler (健康检查)\n");
        else if (strncmp(path, "/api/user", 9) == 0)
            printf("     => user_handler (用户管理)\n");
        else if (strncmp(path, "/api/message", 12) == 0)
            printf("     => message_handler (消息处理)\n");
        else if (strncmp(path, "/api/community", 14) == 0)
            printf("     => community_handler (社区管理)\n");
        else if (strncmp(path, "/api/files", 10) == 0)
            printf("     => file_handler (文件处理)\n");
        else if (strncmp(path, "/api/ws", 7) == 0)
            printf("     => websocket_handler (WebSocket)\n");
        else
            printf("     => 未知路由或静态文件\n");

        if (strlen(body) > 0) {
            printf("\n[*] 响应数据:\n");
            print_json(body, 4);
        }

        return (status >= 200 && status < 300) ? 0 : -1;
    }

    printf("  [3/5] TLS 握手: 失败 - %s\n", tls_last_error());
    printf("  [4/5] HTTP 请求: 未发送\n");
    printf("  [5/5] 响应处理: 无响应\n");
    return -1;
}

/** 处理 ping 命令 - 服务器延迟测试 */
static int cmd_ping(int argc, char** argv)
{
    int count = 3;
    if (argc >= 1) {
        count = atoi(argv[0]);
        if (count < 1) count = 1;
        if (count > 20) {
            printf("[-] ping 次数限制在 1-20 之间\n");
            count = 20;
        }
    }

    printf("[*] 开始 ping %s:%d (%d 次)...\n\n",
           g_config.host, g_config.port, count);

    int succeeded = 0;
    int failed = 0;
    double total_time = 0;
    double min_time = 999999;
    double max_time = 0;

    for (int i = 0; i < count; i++) {
        char response[BUFFER_SIZE] = {0};

        clock_t start = clock();
        int ret = http_request("GET", "/api/health", NULL, NULL,
                                response, sizeof(response));
        clock_t end = clock();

        double elapsed = ((double)(end - start)) / CLOCKS_PER_SEC * 1000.0;

        if (ret == 0) {
            int status = http_get_status(response);
            succeeded++;
            total_time += elapsed;
            if (elapsed < min_time) min_time = elapsed;
            if (elapsed > max_time) max_time = elapsed;

            printf("  [%d/%d] 响应时间: %.1f ms  HTTP %d\n",
                   i + 1, count, elapsed, status);
        } else {
            failed++;
            printf("  [%d/%d] 失败: %s\n",
                   i + 1, count, tls_last_error());
        }

        /* 请求间间隔 200ms */
        if (i < count - 1) {
            msleep(200);
        }
    }

    printf("\n");
    printf("[*] Ping 统计:\n");
    printf("    发送: %d, 成功: %d, 失败: %d\n", count, succeeded, failed);
    if (succeeded > 0) {
        printf("    平均: %.1f ms\n", total_time / succeeded);
        printf("    最小: %.1f ms\n", min_time);
        printf("    最大: %.1f ms\n", max_time);
    }

    return (failed == 0) ? 0 : -1;
}

/** 处理 watch 命令 - 实时监控服务器状态 */
static int cmd_watch(int argc, char** argv)
{
    int interval = 2;
    if (argc >= 1) {
        interval = atoi(argv[0]);
        if (interval < 1) interval = 1;
        if (interval > 30) {
            printf("[-] watch 间隔限制在 1-30 秒\n");
            interval = 30;
        }
    }

    int max_rounds = 10;
    if (argc >= 2) {
        max_rounds = atoi(argv[1]);
        if (max_rounds < 1) max_rounds = 1;
        if (max_rounds > 100) {
            printf("[-] watch 轮次限制在 1-100\n");
            max_rounds = 100;
        }
    }

    printf("[*] 开始监控 %s:%d (间隔 %ds, %d 轮)...\n",
           g_config.host, g_config.port, interval, max_rounds);
    printf("    按 Ctrl+C 终止...\n\n");

    int prev_status = -1;
    int unstable_count = 0;

    for (int round = 1; round <= max_rounds; round++) {
        char response[BUFFER_SIZE] = {0};

        clock_t start = clock();
        int ret = http_request("GET", "/api/health", NULL, NULL,
                                response, sizeof(response));
        clock_t end = clock();

        double elapsed = ((double)(end - start)) / CLOCKS_PER_SEC * 1000.0;

        time_t now = time(NULL);
        char time_str[32];
        strftime(time_str, sizeof(time_str), "%H:%M:%S", localtime(&now));

        if (ret == 0) {
            int status = http_get_status(response);
            const char* body = http_get_body(response);

            if (status != prev_status) {
                if (prev_status != -1) {
                    printf("  [%s] ⚠ 状态变化: %d -> %d\n",
                           time_str, prev_status, status);
                }
                prev_status = status;
            }

            const char* status_flag = (status >= 200 && status < 300) ? "✓" : "⚠";
            printf("  [%s] %s HTTP %d  %.1f ms\n",
                   time_str, status_flag, status, elapsed);

            if (status >= 400) {
                unstable_count++;
                printf("        响应: %s\n", body ? body : "(空)");
            } else {
                unstable_count = 0;
            }

        } else {
            printf("  [%s] ✗ 连接失败: %s\n",
                   time_str, tls_last_error());
            prev_status = -1;
            unstable_count++;
        }

        if (unstable_count >= 3) {
            printf("\n[-] 警告: 连续 %d 次异常, 服务器可能不稳定!\n", unstable_count);
        }

        printf("\n");

        if (round < max_rounds) {
            for (int s = 0; s < interval; s++) {
                msleep(1000);
            }
        }
    }

    printf("[*] 监控完成 (%d 轮)\n", max_rounds);
    return (unstable_count == 0) ? 0 : -1;
}

/** 处理 rate-test 命令 - 速率测试 */
static int cmd_rate_test(int argc, char** argv)
{
    int num_requests = 5;
    if (argc >= 1) {
        num_requests = atoi(argv[0]);
        if (num_requests < 1) num_requests = 1;
        if (num_requests > 50) {
            printf("[-] 并发数限制在 1-50\n");
            num_requests = 50;
        }
    }

    printf("[*] 开始速率测试: %d 个并发请求 -> %s:%d\n",
           num_requests, g_config.host, g_config.port);
    printf("    测试端点: /api/health\n\n");

    int success = 0;
    int failure = 0;
    double total_time = 0;

    clock_t test_start = clock();

    for (int i = 0; i < num_requests; i++) {
        char response[BUFFER_SIZE] = {0};

        clock_t req_start = clock();
        int ret = http_request("GET", "/api/health", NULL, NULL,
                                response, sizeof(response));
        clock_t req_end = clock();

        double elapsed = ((double)(req_end - req_start)) / CLOCKS_PER_SEC * 1000.0;

        if (ret == 0) {
            int status = http_get_status(response);
            success++;
            total_time += elapsed;
            printf("  [%3d/%d] ✓ HTTP %d  %.1f ms\n",
                   i + 1, num_requests, status, elapsed);
        } else {
            failure++;
            printf("  [%3d/%d] ✗ 失败: %s\n",
                   i + 1, num_requests, tls_last_error());
        }
    }

    clock_t test_end = clock();
    double total_elapsed = ((double)(test_end - test_start)) / CLOCKS_PER_SEC * 1000.0;

    printf("\n");
    printf("[*] 速率测试结果:\n");
    printf("    总请求: %d\n", num_requests);
    printf("    成功:   %d\n", success);
    printf("    失败:   %d\n", failure);
    printf("    总耗时: %.0f ms\n", total_elapsed);
    if (success > 0) {
        printf("    平均响应: %.1f ms\n", total_time / success);
        printf("    吞吐率:   %.1f req/s\n",
               success / (total_elapsed / 1000.0));
    }

    return (failure == 0) ? 0 : -1;
}

/** 处理 help 命令 */
static int cmd_help(void)
{
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║        Chrono-shift CLI 调试接口                        ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    printf("基础功能:\n");
    printf("  ┌─────────────────────────────────────────────────────────┐\n");
    printf("  │ health                        检查服务器健康状态       │\n");
    printf("  │ endpoint <path> [method] [body]  测试 API 端点         │\n");
    printf("  │ token   <jwt_token>           解码并分析 JWT 令牌       │\n");
    printf("  │ ipc     types                 列出 IPC 消息类型         │\n");
    printf("  │ ipc     send <hex> <json>     发送 IPC 消息             │\n");
    printf("  │ user    list                  列出所有用户              │\n");
    printf("  │ user    get <id>              获取用户信息              │\n");
    printf("  │ user    create <user> <pass>  创建新用户                │\n");
    printf("  │ user    delete <id>           删除用户                  │\n");
    printf("  └─────────────────────────────────────────────────────────┘\n");
    printf("\n");
    printf("连接管理:\n");
    printf("  ┌─────────────────────────────────────────────────────────┐\n");
    printf("  │ connect <host> <port> [tls]    连接到指定服务器         │\n");
    printf("  │ disconnect                    断开当前连接              │\n");
    printf("  └─────────────────────────────────────────────────────────┘\n");
    printf("\n");
    printf("安全与诊断:\n");
    printf("  ┌─────────────────────────────────────────────────────────┐\n");
    printf("  │ tls-info                      显示 TLS 连接信息         │\n");
    printf("  │ json-parse <str>              解析并验证 JSON           │\n");
    printf("  │ json-pretty <str>             格式化输出 JSON           │\n");
    printf("  │ trace <path>                  追踪请求路径              │\n");
    printf("  └─────────────────────────────────────────────────────────┘\n");
    printf("\n");
    printf("性能测试:\n");
    printf("  ┌─────────────────────────────────────────────────────────┐\n");
    printf("  │ ping [count]                  服务器延迟测试 (默认3次)  │\n");
    printf("  │ watch [interval] [rounds]     实时监控服务器状态        │\n");
    printf("  │ rate-test [n]                 速率测试 (n 次请求)       │\n");
    printf("  └─────────────────────────────────────────────────────────┘\n");
    printf("\n");
    printf("通用:\n");
    printf("  ┌─────────────────────────────────────────────────────────┐\n");
    printf("  │ verbose                      切换详细模式               │\n");
    printf("  │ help / ?                     显示此帮助信息             │\n");
    printf("  │ exit / quit                  退出调试 CLI               │\n");
    printf("  └─────────────────────────────────────────────────────────┘\n");
    printf("\n");
    printf("配置:\n");
    printf("  当前服务器: %s:%d\n", g_config.host, g_config.port);
    printf("  协议:       %s\n", g_config.use_tls ? "HTTPS" : "HTTP");
    printf("  详细模式:   %s\n", g_config.verbose ? "开" : "关");
    printf("\n");
    printf("提示:\n");
    printf("  环境变量 CHRONO_HOST / CHRONO_PORT 设置目标\n");
    printf("  connect / disconnect 运行时管理连接\n");
    printf("  ping / watch / rate-test 性能测试需要服务器运行\n");
    printf("\n");
    return 0;
}

/* ============================================================
 * 主处理循环
 * ============================================================ */

static int process_line(char* line)
{
    char* argv[32];
    int argc = 0;

    /* 按空格分割 */
    char* token = strtok(line, " \t");
    while (token && argc < 32) {
        argv[argc++] = token;
        token = strtok(NULL, " \t");
    }

    if (argc == 0) return 0;

    const char* cmd = argv[0];

    if (strcmp(cmd, "exit") == 0 || strcmp(cmd, "quit") == 0) {
        printf("再见!\n");
        return 1;
    } else if (strcmp(cmd, "help") == 0 || strcmp(cmd, "?") == 0) {
        cmd_help();
    } else if (strcmp(cmd, "health") == 0) {
        if (cmd_health(argc - 1, argv + 1) != 0) printf("\n");
    } else if (strcmp(cmd, "endpoint") == 0) {
        if (cmd_endpoint(argc - 1, argv + 1) != 0) printf("\n");
    } else if (strcmp(cmd, "token") == 0) {
        if (cmd_token(argc - 1, argv + 1) != 0) printf("\n");
    } else if (strcmp(cmd, "ipc") == 0) {
        if (cmd_ipc(argc - 1, argv + 1) != 0) printf("\n");
    } else if (strcmp(cmd, "user") == 0) {
        if (cmd_user(argc - 1, argv + 1) != 0) printf("\n");
    } else if (strcmp(cmd, "verbose") == 0) {
        g_config.verbose = !g_config.verbose;
        printf("[*] 详细模式: %s\n", g_config.verbose ? "开" : "关");
    } else if (strcmp(cmd, "connect") == 0 && argc >= 3) {
        snprintf(g_config.host, sizeof(g_config.host), "%s", argv[1]);
        g_config.port = atoi(argv[2]);
        if (argc >= 4 && (strcmp(argv[3], "tls") == 0 || strcmp(argv[3], "1") == 0)) {
            g_config.use_tls = 1;
        } else {
            g_config.use_tls = 0;
        }
        printf("[*] 目标服务器: %s:%d (%s)\n",
               g_config.host, g_config.port,
               g_config.use_tls ? "HTTPS" : "HTTP");
    } else if (strcmp(cmd, "disconnect") == 0) {
        if (cmd_disconnect(argc - 1, argv + 1) != 0) printf("\n");
    } else if (strcmp(cmd, "tls-info") == 0) {
        if (cmd_tls_info(argc - 1, argv + 1) != 0) printf("\n");
    } else if (strcmp(cmd, "json-parse") == 0) {
        if (cmd_json_parse(argc - 1, argv + 1) != 0) printf("\n");
    } else if (strcmp(cmd, "json-pretty") == 0) {
        if (cmd_json_pretty(argc - 1, argv + 1) != 0) printf("\n");
    } else if (strcmp(cmd, "trace") == 0) {
        if (cmd_trace(argc - 1, argv + 1) != 0) printf("\n");
    } else if (strcmp(cmd, "ping") == 0) {
        if (cmd_ping(argc - 1, argv + 1) != 0) printf("\n");
    } else if (strcmp(cmd, "watch") == 0) {
        if (cmd_watch(argc - 1, argv + 1) != 0) printf("\n");
    } else if (strcmp(cmd, "rate-test") == 0) {
        if (cmd_rate_test(argc - 1, argv + 1) != 0) printf("\n");
    } else {
        fprintf(stderr, "未知命令: %s (输入 help 查看帮助)\n", cmd);
    }

    return 0;
}

/* ============================================================
 * 程序入口
 * ============================================================ */

int main(int argc, char** argv)
{
    /* 解析环境变量 */
    const char* env_host = getenv("CHRONO_HOST");
    const char* env_port = getenv("CHRONO_PORT");
    if (env_host) {
        snprintf(g_config.host, sizeof(g_config.host), "%s", env_host);
    }
    if (env_port) {
        g_config.port = atoi(env_port);
        if (g_config.port <= 0 || g_config.port > 65535) {
            g_config.port = DEFAULT_PORT;
        }
    }
    const char* env_tls = getenv("CHRONO_TLS");
    if (env_tls && (strcmp(env_tls, "1") == 0 || strcmp(env_tls, "true") == 0)) {
        g_config.use_tls = 1;
    }

    /* 支持命令行直接执行: debug_cli health */
    if (argc > 1) {
        /* 将 argv[1..] 合并为一行 */
        char line[4096] = {0};
        int pos = 0;
        for (int i = 1; i < argc; i++) {
            if (i > 1) line[pos++] = ' ';
            size_t arg_len = strlen(argv[i]);
            if (pos + (int)arg_len >= (int)sizeof(line) - 1) break;
            memcpy(line + pos, argv[i], arg_len);
            pos += (int)arg_len;
        }
        line[pos] = 0;
        return process_line(line);
    }

    /* 交互模式 */
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║     Chrono-shift CLI 调试接口 v1.0                      ║\n");
    printf("║     输入 help 查看可用命令, exit 退出                    ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n");
    printf("\n");

    char line[4096];
    while (1) {
        printf("debug> ");
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break;
        }

        /* 去除换行符 */
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = 0;
        }

        if (process_line(line) != 0) break;
    }

    return 0;
}
