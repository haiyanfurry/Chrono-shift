/**
 * Chrono-shift 客户端网络层 (已拆分)
 * 语言标准: C99
 *
 * 本文件已拆分为以下子模块:
 *   - net_tcp.c:   Winsock 初始化/清理、TCP/TLS 连接管理、自动重连
 *   - net_http.c:  HTTP/1.1 请求构建与响应解析
 *   - net_ws.c:    WebSocket (RFC 6455) 握手与帧编解码
 *   - net_sha1.c:  SHA-1 哈希 + Base64 编码 (WebSocket 握手用)
 *
 * 公共 API 保持不变 (network.h)
 * 内部接口见 net_core.h
 */

#include "network.h"
