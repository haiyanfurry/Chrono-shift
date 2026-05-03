/**
 * init_commands.cpp — 初始化所有 CLI 命令模块 (C++23 版本)
 *
 * 此文件统一声明并调用所有 cmd_*.c 中的 init_cmd_*() 函数。
 * 由于 cmd_*.c 仍为 C 语言文件，所有 extern 声明使用 extern "C" 链接。
 * main.cpp 中调用 init_commands() 完成命令注册。
 */
#include "../devtools_cli.hpp"

// ============================================================
// 声明所有命令模块的初始化函数 (C linkage — 与 cmd_*.c 链接)
// ============================================================

extern "C" {

/* 基础功能 */
extern int init_cmd_health(void);
extern int init_cmd_endpoint(void);
extern int init_cmd_token(void);
extern int init_cmd_ipc(void);
extern int init_cmd_user(void);

/* 客户端本地命令 */
extern int init_cmd_session(void);
extern int init_cmd_config(void);
extern int init_cmd_storage(void);
extern int init_cmd_crypto(void);
extern int init_cmd_network(void);

/* WebSocket 调试 */
extern int init_cmd_ws(void);

/* 数据库操作 */
extern int init_cmd_msg(void);
extern int init_cmd_friend(void);
extern int init_cmd_db(void);

/* 连接管理 */
extern int init_cmd_connect(void);
extern int init_cmd_disconnect(void);

/* 安全与诊断 */
extern int init_cmd_tls_info(void);
extern int init_cmd_gen_cert(void);
extern int init_cmd_json_parse(void);
extern int init_cmd_json_pretty(void);
extern int init_cmd_trace(void);
extern int init_cmd_obfuscate(void);

/* 性能测试 */
extern int init_cmd_ping(void);
extern int init_cmd_watch(void);
extern int init_cmd_rate_test(void);

/* I2P + 社交 (C++23) */
extern int init_cmd_i2p(void);
extern int init_cmd_social(void);

} // extern "C"

// ============================================================
// init_commands — 注册所有命令
// ============================================================
void init_commands(void)
{
    /* 基础功能 */
    init_cmd_health();
    init_cmd_endpoint();
    init_cmd_token();
    init_cmd_ipc();
    /* init_cmd_user(); — 替换为 cmd_social.cpp 的 "uid" 命令 */

    /* 客户端本地命令 */
    init_cmd_session();
    init_cmd_config();
    init_cmd_storage();
    init_cmd_crypto();
    init_cmd_network();

    /* WebSocket 调试 */
    init_cmd_ws();

    /* 数据库操作 */
    /* init_cmd_msg(); — 替换为 cmd_social.cpp 的 "msg" 命令 */
    /* init_cmd_friend(); — 替换为 cmd_social.cpp 的 "friend" 命令 */
    init_cmd_db();

    /* 连接管理 */
    init_cmd_connect();
    init_cmd_disconnect();

    /* 安全与诊断 */
    init_cmd_tls_info();
    init_cmd_gen_cert();
    init_cmd_json_parse();
    init_cmd_json_pretty();
    init_cmd_trace();
    init_cmd_obfuscate();

    /* 性能测试 */
    init_cmd_ping();
    init_cmd_watch();
    init_cmd_rate_test();

    /* I2P + 社交 */
    init_cmd_i2p();
    init_cmd_social();
}
