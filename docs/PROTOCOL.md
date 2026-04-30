# Chrono-shift 通信协议文档

## 传输层

- HTTP/1.1 REST API
- WebSocket (RFC 6455) 用于实时通信
- TLS 加密

## WebSocket 消息格式

```json
{
  "type": "message_type",
  "data": {},
  "timestamp": 1700000000000
}
```

## 二进制协议头 (8字节)

| 偏移 | 大小 | 说明 |
|------|------|------|
| 0    | 4    | 魔数 0x43485346 ('CHSF') |
| 4    | 1    | 协议版本 0x01 |
| 5    | 1    | 消息类型 |
| 6    | 2    | 消息体长度 (网络字节序) |

*Phase 2 中完善协议细节*
