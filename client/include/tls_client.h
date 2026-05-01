#ifndef CHRONO_TLS_CLIENT_H
#define CHRONO_TLS_CLIENT_H

/**
 * Chrono-shift 客户端 TLS 抽象层
 * 基于 OpenSSL 的跨平台 TLS 封装
 * 语言标准: C99
 *
 * 为客户端和 CLI 工具提供 TLS 连接能力:
 * 1. 客户端 network/ — 连接 HTTPS 服务端
 * 2. CLI 工具 debug_cli.c — 连接 HTTPS 服务端
 */

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

/* ============================================================
 * Opaque 类型 (隐藏 OpenSSL 实现细节)
 * ============================================================ */

/* 前向声明: 实际类型在 .c 文件中定义 */
typedef struct ssl_st SSL;
typedef struct ssl_ctx_st SSL_CTX;

/* ============================================================
 * 客户端 API
 * ============================================================ */

/**
 * 初始化客户端 TLS 上下文 (只需一次)
 * @param ca_file 可选: CA 证书路径 (PEM), NULL=使用系统信任库
 * @return 0=成功, -1=失败
 */
int tls_client_init(const char* ca_file);

/**
 * 连接到远程 TLS 服务器
 * 执行 TCP 连接 + TLS 握手
 * @param ssl_out   输出: SSL 对象指针
 * @param host      服务器域名 (用于 SNI)
 * @param port      服务器端口
 * @return socket 描述符, 失败返回 -1
 */
int tls_client_connect(SSL** ssl_out, const char* host, uint16_t port);

/**
 * 关闭 TLS 连接并释放 SSL 对象
 * @param ssl SSL 对象指针 (可为 NULL)
 */
void tls_close(SSL* ssl);

/**
 * 清理客户端 TLS 上下文
 */
void tls_client_cleanup(void);

/* ============================================================
 * 通用 I/O API
 * ============================================================ */

/**
 * 从 TLS 连接读取数据 (等同 recv, 但通过 SSL 解密)
 * @param ssl    SSL 对象
 * @param buf    接收缓冲区
 * @param len    缓冲区大小
 * @return 实际读取字节数, <=0 表示错误或关闭
 */
int tls_read(SSL* ssl, void* buf, int len);

/**
 * 向 TLS 连接写入数据 (等同 send, 但通过 SSL 加密)
 * @param ssl    SSL 对象
 * @param buf    发送数据
 * @param len    数据长度
 * @return 实际发送字节数, <=0 表示错误
 */
int tls_write(SSL* ssl, const void* buf, int len);

/**
 * 获取 TLS 连接信息 (用于调试输出)
 * @param ssl        SSL 对象
 * @param out_buf    输出缓冲区
 * @param buf_size   缓冲区大小
 */
void tls_get_info(SSL* ssl, char* out_buf, size_t buf_size);

/**
 * 获取最后错误描述 (线程安全)
 */
const char* tls_last_error(void);

#endif /* CHRONO_TLS_CLIENT_H */
