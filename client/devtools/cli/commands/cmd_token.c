/**
 * cmd_token.c - JWT 令牌解码命令
 * 对应 debug_cli.c:603 cmd_token
 */
#include "../devtools_cli.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>

/** 解码 JWT 的单个部分 (Base64 -> JSON 打印) */
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

/** token - 解码并分析 JWT */
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

void init_cmd_token(void)
{
    register_command("token",
                     "解码并分析 JWT 令牌",
                     "token <jwt_token>",
                     cmd_token);
}
