#ifndef CHRONO_TLS_SERVER_H
#define CHRONO_TLS_SERVER_H

/**
 * Chrono-shift TLS 抽象层
 * 基于 OpenSSL 的跨平台 TLS 封装
 * 语言标准: C99
 *
 * 为以下三个场景提供统一 TLS 接口:
 * 1. 服务端 http_server.c — 接收 TLS 连接
 * 2. CLI 调试工具 debug_cli.c — 连接 HTTPS 服务端
 * 3. 客户端 network.c — 连接 HTTPS 服务端
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
 * 服务端 API (用于 http_server.c)
 * ============================================================ */

/**
 * 初始化服务端 TLS 上下文
 * @param cert_file  证书文件路径 (PEM, 通常为 fullchain.pem)
 * @param key_file   私钥文件路径 (PEM, 通常为 privkey.pem)
 * @return 0=成功, -1=失败
 */
int tls_server_init(const char* cert_file, const char* key_file);

/**
 * 将已连接的 socket 封装为 TLS 连接 (服务端模式)
 * 调用后使用 tls_read/tls_write 进行加密通信
 * @param fd 已 accept() 的 socket 描述符
 * @return SSL* 指针, 失败返回 NULL
 */
SSL* tls_server_wrap(int fd);

/**
 * 关闭 TLS 连接并释放 SSL 对象
 * @param ssl SSL 对象指针 (可为 NULL)
 */
void tls_close(SSL* ssl);

/**
 * 自动初始化 TLS（检查现有证书，或自动生成自签名证书）
 * @param cert_dir 证书存储目录 (如 "./certs")
 * @return 0=成功, -1=失败
 */
int tls_server_auto_init(const char* cert_dir);

/**
 * 清理全局服务端 TLS 上下文
 */
void tls_server_cleanup(void);

/**
 * 检查服务端 TLS 是否已初始化
 */
bool tls_server_is_enabled(void);

/* ============================================================
 * 客户端 API (用于 debug_cli.c, network.c)
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
 * 清理客户端 TLS 上下文
 */
void tls_client_cleanup(void);

/* ============================================================
 * 通用 I/O API (服务端和客户端共用)
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

#endif /* CHRONO_TLS_SERVER_H */
