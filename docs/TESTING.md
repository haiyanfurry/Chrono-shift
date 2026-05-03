# Chrono-shift 测试指南 v3.0.0

## 冒烟测试

```bash
cd client/build
echo "help" | ./chrono-client.exe
# 预期: 显示所有 22 个命令
echo "health" | ./chrono-client.exe
# 预期: [*] 检查服务器健康状态
```

## 单元测试 (CLI 命令)

| 命令 | 测试方法 | 预期结果 |
|------|---------|---------|
| `help` | 直接运行 | 显示命令列表 |
| `config show` | 直接运行 | 显示当前配置 |
| `crypto test` | 直接运行 | AES-256-GCM 加密/解密测试通过 |
| `ipc types` | 直接运行 | 列出 IPC 消息类型 |
| `gen-cert` | 直接运行 | 生成自签名证书 |
| `ping 127.0.0.1` | 本地测试 | 延迟报告 |
| `token <jwt>` | 提供测试 JWT | 解码并显示载荷 |

## 安全测试

```bash
# 运行安全测试套件
bash tests/security_pen_test.sh
bash tests/asm_obfuscation_test.sh
bash tests/api_verification_test.sh
bash tests/loopback_test.sh
```

## I2P 集成测试

```bash
# 1. 确保 I2P 路由器运行
cd /path/to/i2p && java -jar i2p.jar &

# 2. 等待路由器就绪 (~2分钟)
sleep 120

# 3. 测试 SAM 连接
./chrono-client.exe <<EOF
i2p start
i2p status
EOF
# 预期: [+] Connected to I2P SAM bridge

# 4. 好友握手测试 (两实例)
# 终端 A:
./chrono-client.exe
> i2p start
> uid set alice
# 记录显示的 .i2p 地址

# 终端 B:
./chrono-client.exe
> i2p start
> uid set bob

# A 发送请求:
> friend add bob@<bob-address>.b32.i2p

# B 收到并接受:
> friend pending
> friend accept alice

# 双向消息:
# A:
> msg chat bob
[bob]> hello!

# B:
> msg chat alice
[alice]> hi!
```

## 性能测试

```bash
# 加密性能
./chrono-client.exe <<EOF
crypto test
EOF

# 网络延迟
./chrono-client.exe <<EOF
ping 127.0.0.1
rate_test 100
EOF
```

## 回归检查清单

- [ ] `chrono-client.exe` 启动无崩溃
- [ ] `help` 显示所有命令
- [ ] `config show` 正常
- [ ] `crypto test` 加密测试通过
- [ ] `gen-cert` 证书生成成功
- [ ] `ping` 网络延迟正常
- [ ] `ipc types` IPC 类型显示
- [ ] I2P: SAM 连接成功
- [ ] I2P: 好友请求/接受/拒绝
- [ ] I2P: 交互式消息聊天
