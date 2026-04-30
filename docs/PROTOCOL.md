# Chrono-shift 通信协议文档

> **版本**: v0.2.0 | **更新**: 2026-04

## 传输层

- **HTTP/1.1 REST API** — 用于用户管理、消息查询、文件操作、模板管理
- **WebSocket** (RFC 6455) — 用于实时消息推送
- **TLS 1.3** — 所有通信强制加密 (HTTPS/WSS only)

## HTTP 请求/响应格式

### 请求头

```http
Content-Type: application/json
Authorization: Bearer <jwt_token>  # 需要认证的接口
```

### 标准响应结构

```json
// 成功
{ "status": "ok", "data": { ... } }

// 错误
{ "status": "error", "message": "错误描述" }

// 列表 (分页)
{ "status": "ok", "data": { "items": [...], "has_more": false } }
```

### HTTP 方法

| 方法 | 用途 |
|------|------|
| `GET` | 查询资源 (用户资料/消息历史/模板列表/文件下载) |
| `POST` | 创建资源 (注册/登录/发送消息/上传文件/上传模板) |
| `PUT` | 更新资源 (更新用户资料) |

## WebSocket 协议

### 连接建立

```
GET /ws?token=<jwt_token> HTTP/1.1
Host: localhost:4443
Upgrade: websocket
Connection: Upgrade
Sec-WebSocket-Key: <base64 random 16 bytes>
Sec-WebSocket-Version: 13
```

服务器响应:

```
HTTP/1.1 101 Switching Protocols
Upgrade: websocket
Connection: Upgrade
Sec-WebSocket-Accept: <sha1 base64 accept key>
```

### WebSocket 帧格式 (RFC 6455)

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|F|R|R|R| opcode|M| Payload len  |    Extended payload len     |
|I|S|S|S|  (4)  |A|    (7)      |   (16/64 if payload_len=126) |
|N|V|V|V|       |S|             |                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|     Extended payload len continued     |   Masking-key (if MASK=1) |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|     Masking-key (continued)            |   Payload Data           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

| 字段 | 说明 |
|------|------|
| `FIN` | 1 = 最后一帧 |
| `opcode` | 0x1 = Text, 0x8 = Close, 0x9 = Ping, 0xA = Pong |
| `MASK` | 客户端→服务器必须为 1 (掩码) |
| `Payload len` | 7/16/64 位长度编码 |

### WebSocket 消息格式

所有 WebSocket 消息使用 JSON 文本帧:

```json
{
  "type": "message_type",
  "data": {},
  "timestamp": 1700000000000
}
```

### 消息类型

| 类型 | 方向 | 说明 |
|------|------|------|
| `message.send` | 客户端→服务端 | 发送新消息 |
| `message.new` | 服务端→客户端 | 推送新消息通知 |
| `message.ack` | 服务端→客户端 | 消息已送达确认 |
| `typing` | 客户端→服务端 | 输入状态通知 |
| `online_status` | 服务端→客户端 | 好友在线状态变更 |

## IPC 协议 (客户端 C ↔ JS 通信)

### 消息帧格式 (8 字节头 + 变长体)

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|          魔数 0x43485346 ('CHSF')                             |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  版本 0x01    |  消息类型     |        消息体长度 (BE)          |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                        消息体 (JSON)                          |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

| 偏移 | 大小 | 说明 |
|------|------|------|
| 0    | 4    | 魔数 `0x43485346` ('CHSF') |
| 4    | 1    | 协议版本 `0x01` |
| 5    | 1    | 消息类型 (见下表) |
| 6    | 2    | 消息体长度 (网络字节序) |
| 8    | 变长 | JSON 消息体 |

### IPC 消息类型

| 类型码 | 名称 | 说明 | 方向 |
|--------|------|------|------|
| `0x01` | `LOGIN` | 用户登录 | JS → C |
| `0x02` | `LOGOUT` | 用户登出 | JS → C |
| `0x03` | `MESSAGE` | 消息发送 | 双向 |
| `0x04` | `MESSAGE_ACK` | 消息确认 | C → JS |
| `0x05` | `SYSTEM_NOTIFY` | 系统通知 | C → JS |
| `0x06` | `HEARTBEAT` | 心跳检测 | 双向 |
| `0x07` | `FILE_TRANSFER` | 文件传输 | 双向 |
| `0x08` | `FRIEND_REQUEST` | 好友请求 | 双向 |
| `0x09` | `FRIEND_RESPONSE` | 好友响应 | 双向 |
| `0x50` | `OPEN_URL` | 打开外部链接 | JS → C |

### 协议实现文件

- C 端: [`ipc_bridge.h`](../client/include/ipc_bridge.h) — 类型枚举 + 编解码函数
- JS 端: [`ipc.js`](../client/ui/js/ipc.js) — 消息序列化/反序列化

## 调试 CLI 协议

[`debug_cli`](../server/tools/debug_cli.c) 支持以下交互协议:

### WebSocket 调试 (E1)

```
ws connect <token> [path]     建立 WebSocket 连接
ws send <json>                通过 WebSocket 发送 JSON 消息
ws recv                       接收 WebSocket 消息 (非阻塞)
ws close                      关闭 WebSocket 连接
ws status                     查看 WebSocket 连接状态
```

WebSocket 握手使用自定义 C 实现的 SHA-1 + Base64 (不依赖 OpenSSL)。

### 数据库操作 (E2)

```
msg list <uid> [limit] [offset]    列出用户消息
msg get <id>                       获取消息详情
msg send <to> <text>               发送测试消息
friend list <uid>                  列出用户好友
friend add <uid1> <uid2>           添加好友关系
db list <type>                     列出数据库内容
```

## 相关源文件

| 文件 | 功能 |
|------|------|
| [`protocol.h`](../server/include/protocol.h) | 协议常量/结构体定义 |
| [`protocol.c`](../server/src/protocol.c) | 协议编解码实现 |
| [`websocket.h`](../server/include/websocket.h) | WebSocket 帧类型枚举 |
| [`websocket.c`](../server/src/websocket.c) | WebSocket 握手/帧处理 |
| [`ipc_bridge.h`](../client/include/ipc_bridge.h) | IPC 消息类型定义 |
| [`ipc.js`](../client/ui/js/ipc.js) | IPC JavaScript 实现 |
