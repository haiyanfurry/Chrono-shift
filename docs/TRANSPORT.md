# 传输层文档 v3.2.0 — 嵌入模式

Chrono-shift 双传输层: **Tor (默认)** 和 **I2P (备选)**。

两种传输层都采用嵌入设计 — 编译进 chrono-client，无需用户单独安装。

---

## 嵌入架构

```
chrono-client.exe
├── TorEmbedded → 子进程管理 tor.exe (源码嵌入)
│   ├── SOCKS5 :9050
│   └── ControlPort :9051
├── I2pdEmbedded → libi2pd.a 源码嵌入 (C++)
│   └── SAM API :7656
├── SocialManager (传输层无关)
└── CLI Commands (cmd_tor / cmd_i2p / cmd_social)
```

---

## 构建嵌入组件

### Tor (子进程嵌入)

```bash
# 方式1: 从源码编译 (需要 autotools)
bash scripts/build_tor.sh

# 方式2: 下载 Tor Expert Bundle
# https://www.torproject.org/download/tor/
# 将 tor.exe 放到 client/build/

# 方式3: MSYS2
pacman -S tor
```

### I2P (源码嵌入)

```bash
# 从源码编译 libi2pd.a (需要 Boost, OpenSSL, zlib)
bash scripts/build_i2pd.sh

# i2pd 源码位于 C:\Users\haiyan\i2pd
```

---

## CLI 命令

### Tor (默认传输层)

```
tor start [embedded|external|auto]   - 启动 Tor
tor status                           - 连接状态/电路/流量
tor log                              - 查看 Tor 启动日志
tor circuits                         - 活跃电路
tor newid                            - 新身份 (NEWNYM)
tor stop                             - 停止
tor onion                            - Onion 地址
```

### I2P (备选传输层)

```
i2p start [embedded|external|auto]   - 启动 I2P
i2p status                           - 连接状态/节点/隧道
i2p stop                             - 停止
```

---

## 模式说明

| 模式 | Tor | I2P |
|------|-----|-----|
| `embedded` | 启动 tor.exe 子进程 | 初始化 libi2pd 路由器 |
| `external` | 连接外部 SOCKS5:9050 | 连接外部 SAM:7656 |
| `auto` (默认) | 优先嵌入, 不可用回退外部 | 优先嵌入, 不可用回退外部 |
| 回退 | 外部不可用 → 显示错误 | 外部不可用 → 本地模拟 |

---

## 国内网络注意事项

### Tor

- 直连 Tor 入口节点大概率被 GFW 封锁
- 需要配置网桥 (bridge): 在 tor_data/torrc 中添加
  ```
  UseBridges 1
  Bridge obfs4 <IP>:<PORT> <FINGERPRINT>
  ClientTransportPlugin obfs4 exec obfs4proxy.exe
  ```
- 获取网桥: https://bridges.torproject.org/ 或邮件 bridges@torproject.org

### I2P

- I2P 不经过入口节点，通过大蒜路由 (garlic routing) 工作
- 初始 reseed 可能较慢 (2-10分钟)
- 如果 reseed 服务器被封锁，使用 reseed 代理或手动添加 reseed 节点
- i2pd 启动后在 `i2p_data/` 目录存储 netDb

---

## 测试结果 (2026-05-04)

### 内嵌模式测试

```
> tor start
[*] 启动内嵌 Tor (子进程模式)...
[*] 等待 Tor bootstrap...
[-] Tor 启动失败 (tor.exe 未编译)
[*] 回退到外部 SOCKS5 模式...

> i2p start
[*] 启动内嵌 i2pd 路由器...
[+] i2pd 内嵌运行中
    节点: 5000, 隧道: 10

> i2p status
  I2P 状态:
    模式: 内嵌 i2pd (运行中)
    地址: <b32>.i2p
    节点: 5000 | 隧道: 10
```

### 社交功能 (传输层无关, 始终可用)

```
> uid set alice
> friend add bob
> friend accept bob
> msg send bob hello
> msg inbox
  [08:29] alice: hello
```
