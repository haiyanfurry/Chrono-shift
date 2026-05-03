# I2P 集成设计 v3.0.0

## 架构

Chrono-shift 通过 **SAM v3 API** 与 I2P 路由器通信:

```
Chrono-shift (CLI) <--TCP:7656--> I2P Router (SAM Bridge) <--I2P Network--> Peer
```

## SAM v3 协议

SAM (Simple Anonymous Messaging) 是 I2P 提供的应用层桥接协议，通过 TCP localhost:7656 访问。

### 会话建立

```
→ HELLO VERSION MIN=3.0 MAX=3.1
← HELLO REPLY RESULT=OK VERSION=3.1

→ SESSION CREATE STYLE=STREAM ID=chrono DESTINATION=transient
← SESSION STATUS RESULT=OK DESTINATION=<our-base64>
```

### 地址解析

```
→ NAMING LOOKUP NAME=<target>.b32.i2p
← NAMING REPLY RESULT=OK NAME=<target> DESTINATION=<base64-dest>
```

### 流连接

```
→ STREAM CONNECT ID=chrono DESTINATION=<base64-dest>
← STREAM STATUS RESULT=OK
(双向字节流已建立)
```

## NAT 穿透

I2P 通过 SSU (Secure Semireliable UDP) 传输层自动处理 NAT 穿透。应用层无需手动实现打洞。

## 实现文件

| 文件 | 说明 |
|------|------|
| `client/src/i2p/SamClient.h` | SAM v3 API 客户端 |
| `client/src/i2p/SamClient.cpp` | 实现 |
| `client/src/i2p/I2pProtocol.h` | 协议适配层 |
| `client/src/i2p/I2pProtocol.cpp` | 复用 protocol.h 二进制帧 |

## 依赖

- I2P 路由器 (Java, >= 2.4.0)
- localhost TCP 端口 7656 (SAM Bridge 默认)
- I2P 路由器需要 ~2 分钟启动和集成到网络

## 安全考量

- SAM Bridge 仅监听 localhost，不暴露到外部网络
- 所有 I2P 流量经过大蒜路由 (garlic routing) 多层加密
- 应用层额外 AES-256-GCM E2E 加密
- .i2p 目标地址本身是加密身份，不泄露 IP
