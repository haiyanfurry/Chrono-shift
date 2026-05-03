# Chrono-shift 安全审计报告 v3.0.0

> 审计日期: 2026-05-04 | 审计范围: 全部源码 | 方法: 静态分析 + 手动审查

## 概览

| 严重度 | 数量 | 说明 |
|--------|------|------|
| CRITICAL | 4 | 可远程触发，可能导���代码执行或系统沦陷 |
| HIGH | 5 | 重大安全漏洞，可导���数据泄露或完整性破坏 |
| MEDIUM | 6 | 中等风险，特定条件下可被利用 |
| LOW | 4 | 低风险，防御性改进 |

---

## CRITICAL 漏洞

### C1. json_parser.c — 堆缓冲区溢出 (CWE-122)

**文件**: `client/src/json_parser.c:78`
**函数**: `json_escape_string`

`sprintf(p, "\\u%04x", c)` 在控制字符转义时未检查剩余缓冲区容量。缓冲区计算公式 `len*2 + 3` 假设每个输入字符最多产生 2 字节输出，但 `<0x20` 字符产生 6 字节 (`\u00XX`)。恶意输入可触发堆溢出。

**修复**: 将 `len*2+3` 改为 `len*6+3`，或改用 `snprintf`。

### C2. HttpConnection.cpp — 无符号整数回绕导致堆溢出 (CWE-190)

**文件**: `client/src/network/HttpConnection.cpp:110`
**函数**: `recv_all`

当 `buf_pos > buffer.size()` 时，`buffer.size() - buf_pos - 1` 作为无符号整数回绕为极大值，导致 `recv()` 写入堆缓冲区之外。

**修复**: 在减法前检查 `buf_pos < buffer.size()`。

### C3. net_http.cpp — 同类型无符号回绕 (CWE-190)

**文件**: `client/devtools/cli/net_http.cpp:131-146`

同样模式：`recv()` 返回值可能造成 `total >= BUFFER_SIZE`，下次迭代计算 `BUFFER_SIZE - 1 - total` 溢出。

**修复**: 在每次迭代前验证 `total < BUFFER_SIZE`。

### C4. ChronoStream v1 ASM 流密码 — 无 IV/无 MAC (CWE-329)

**文件**: `client/security/asm/obfuscate.asm`

自研流密码存在严重设计缺陷：
- **无 IV/Nonce**: 同一密钥加密两条消息产生相同密钥流 → two-time-pad 攻击
- **无 MAC**: 密文可被篡改且无法检测
- **RC4 类设计**: RC4 的已知偏差可能适用
- **"512 位密钥"具误导性**: 内部状态仅 264 字节

**修复**: 弃用此自研密码。仅使用 AES-256-GCM。

---

## HIGH 漏洞

### H1. cmd_msg.c / cmd_friend.c / cmd_user.c — JSON 注入 (CWE-74)

**文件**: `client/devtools/cli/commands/cmd_msg.c:71`, `cmd_friend.c:55`, `cmd_user.c:81`

通过 `snprintf` 字符串拼接构建 JSON，用户输入中的 `"` 未转义：

```c
snprintf(body, ..., "{\"to_user_id\":%s,\"content\":\"%s\"}", to_uid, text);
// attacker input: text = \",\"admin\":true}
// result: {"to_user_id":1,"content":"","admin":true}}
```

**修复**: 使用 `json_escape_string()` 或 `json_build_response()` 构建 JSON。

### H2. cmd_gen_cert.c — 命令注入 (CWE-78)

**文件**: `client/devtools/cli/commands/cmd_gen_cert.c:210`

用户提供的 CN 参数直接插入 `system()` 调用：
```c
snprintf(cmd, ..., "... -subj \"/CN=%s/...\" ...", cn);
```

`cn` 包含 `"; rm -rf /;"` 可执行任意命令。

**修复**: 调用 `system()` 前验证 CN 仅含安全字符 (字母数字和 `.` `-` `*`)。

### H3. crypto.rs — "Keypair" 实为对称密钥 (CWE-324)

**文件**: `client/security/src/crypto.rs:20`

`generate_keypair()` 生成的是单个 AES-256 对称密钥，不是非对称密钥对。标注为 "public key" 具有误导性。无 ECDH 密钥交换，无前向安全性。

**修复**: 重命名为 `generate_symmetric_key()`；实现真正的 ECDH 密钥交换。

### H4. obfuscate.asm — 加解密为同一函数 (CWE-328)

加密和解密使用相同代码 (`asm_deobfuscate: jmp asm_obfuscate`)，确认这是纯 XOR 流密码 — 可篡改。

**修复**: 同 C4，弃用此密码。

### H5. 协议无消息认证 (CWE-345)

**文件**: `client/include/protocol.h`

二进制协议头缺少序列号和 HMAC：
- 无重放保护
- 无消息完整性校验
- 攻击者可注入/重排/重放数据包

**修复**: 添加 4 字节序列号和 16 字节 HMAC-SHA256 到协议头。

---

## MEDIUM 漏洞

### M1. WebSocketClient — 无界递归 (CWE-674)

**文件**: `client/src/network/WebSocketClient.cpp:310`

收到 Ping 帧后递归调用自身。恶意对等端发送无限 Ping 流可耗尽栈空间。

**修复**: 改用循环代替递归处理控制帧。

### M2. cmd_session.c — 令牌泄露到终端 (CWE-532)

**文件**: `client/devtools/cli/commands/cmd_session.c:31`

`session_token` 完整打印到终端，日志或屏幕截图可能泄露。

**修复**: 显示时截断令牌 (前 8 字符 + `...`)。

### M3. json_parser.c — 截断 `\u` 转义越界读 (CWE-126)

**文件**: `client/src/json_parser.c:128-137`

`\uXXXX` 处理器在读取 4 个十六进制数字前未检查字符串结尾。输入 `"\u` (截断) 导致越界读取。

**修复**: 每次 `p->pos++` 前检查 `p->s[p->pos] != '\0'`。

### M4. cmd_ws.c — WebSocket 密钥可预测 (CWE-338)

**文件**: `client/devtools/cli/commands/cmd_ws.c:160`

`srand(time(NULL))` 后 `rand() % 256` 生成密钥。时间戳可预测。

**修复**: 使用 `BCryptGenRandom` 或 `OsRng` 生成密钥。

### M5. HttpConnection.cpp — Content-Length 整数截断 (CWE-197)

**文件**: `client/src/network/HttpConnection.cpp:152`

`std::atol()` 返回 32 位 `long`，立即转换为 `size_t`。超过 2^31 的值被截断。

**修复**: 使用 `std::strtoull` 解析为 64 位。

### M6. TlsWrapper — 无证书固定

**文件**: `client/src/network/TlsWrapper.cpp`

完全依赖系统 CA 信任库，无证书固定或主机名验证钩子。

**修复**: 添加证书固定检查。

---

## LOW 发现

### L1. tls_client.c — 套接字类型不匹配 (Windows)

**文件**: `client/src/network/tls_client.c:247`

`SOCKET` (无符号) 强制转换为 `int`。`INVALID_SOCKET` 检查可能失败。

### L2. TcpConnection.cpp — gethostbyname 非线程安全

**文件**: `client/src/network/TcpConnection.cpp:170`

应使用 `getaddrinfo` 替代已弃用的 `gethostbyname`。

### L3. cmd_connect.c — atoi 无法区分错误

**文件**: `client/devtools/cli/commands/cmd_connect.c:20`

`atoi("abc")` 返回 0，与有效输入 "0" 不可区分。

### L4. cmd_token.c — ctime 非线程安全

**文件**: `client/devtools/cli/commands/cmd_token.c:103`

---

## 代码质量问题

### 死代码

| 文件/目录 | 原因 |
|-----------|------|
| `client/src/app/` (全部) | GUI 组件，CLI 不需要 |
| `client/devtools/core/DevToolsIpcHandler.cpp` | WebView2 IPC 桥接 |
| `client/devtools/ui/` | Web devtools 面板 |
| `client/include/webview_manager.h` | WebView2 接口 |

### 屎山代码模式

1. **双重 C/C++ 实现**: 22 命令 × 2 = 44 文件，需同步维护
2. **内联 base64**: `main.cpp:253-295`，43 行应在独立文件
3. **MOCK 加密回退**: `CryptoEngine.cpp` 在生产构建中应编译报错
4. **栈分配过量**: `char response[65536]` 在每个命令函数中，深度嵌套可能栈溢出
5. **全局可变状态**: `g_config`, `g_command_table` 无同步机制

---

## 修复优先级

| 优先级 | 漏洞 | 预计工时 |
|--------|------|---------|
| P0 | C1: json_escape_string 溢出 | 0.5h |
| P0 | C2: HttpConnection 无符号回绕 | 0.5h |
| P0 | C3: net_http 无符号回绕 | 0.5h |
| P1 | H1: JSON 注入 (3 文件) | 1h |
| P1 | H2: 命令注入 (gen_cert) | 0.5h |
| P1 | H5: 协议无认证 | 2h |
| P2 | M1: WebSocket 递归 | 0.5h |
| P2 | M2: 令牌泄露 | 0.2h |
| P2 | M3: JSON 越界读 | 0.5h |
| P3 | H3/H4/C4: 自研密码问题 | 文档化，长期替换 |

---

## 合规性

- **OWASP Top 10 (2021)**: 覆盖 A02(加密失效), A03(注入), A04(不安全设计), A06(脆弱组件)
- **CWE Top 25**: 覆盖 CWE-122, CWE-190, CWE-74, CWE-78, CWE-329, CWE-338, CWE-345, CWE-674
- **NIST SP 800-53**: 对应 AC(访问控制), IA(身份认证), SC(系统与通信保护), SI(系统与信息完整性)
