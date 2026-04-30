# HTTPS 迁移技术指南

> **文档版本**: 2.0.0
> **适用项目**: Chrono-shift (墨竹)
> **目标**: 将 HTTP 明文通信迁移至 TLS 加密通信，消除中间人攻击 (MITM) 途径
> **状态**: ✅ TLS/HTTPS 支持已内置实现，详见第 2.4 节
> **占位符说明**: 本文档使用 `example.com` 作为域名、`192.0.2.1` 作为服务器 IP，操作时请替换为实际值

---

## 目录

1. [第一阶段：证书获取](#第一阶段证书获取)
2. [第二阶段：服务器端配置](#第二阶段服务器端配置)
   - [2.4 原生 C 服务器 TLS 支持（已实现）](#24-原生-c-服务器-tls-支持已实现)
   - [2.5 CLI 调试工具 HTTPS 支持](#25-cli-调试工具-https-支持)
   - [2.6 客户端 TLS 支持](#26-客户端-tls-支持)
3. [第三阶段：客户端信任配置](#第三阶段客户端信任配置)
4. [第四阶段：端到端测试与验证](#第四阶段端到端测试与验证)
5. [附录：常见错误速查](#附录常见错误速查)

---

## 第一阶段：证书获取

### 1.1 方案选择

| 方案 | 适用场景 | 费用 | 有效期 |
|------|----------|------|--------|
| Let's Encrypt (Certbot) | 生产环境，公网域名 | 免费 | 90 天（可自动续期） |
| 商业 CA（DigiCert/GlobalSign 等） | 企业生产环境，需要 OV/EV | 付费 | 1-2 年 |
| 自签名证书 | 开发/测试环境，内网 | 免费 | 自定（通常 365+ 天） |

### 1.2 Let's Encrypt — Certbot 自动申请（HTTP-01 验证）

#### 前置条件

- 域名 `example.com` 已解析到服务器 IP `192.0.2.1`
- 服务器 80 端口（HTTP）可从公网访问
- 已安装 `snapd`（推荐）或 `pip`

#### 安装 Certbot

```bash
# Ubuntu/Debian（通过 snap，推荐）
sudo snap install core; sudo snap refresh core
sudo snap install --classic certbot
sudo ln -s /snap/bin/certbot /usr/bin/certbot

# CentOS/RHEL
sudo dnf install epel-release
sudo dnf install certbot

# macOS（Homebrew，仅测试用）
brew install certbot
```

#### 申请证书

```bash
sudo certbot certonly --standalone \
  --domain example.com \
  --domain www.example.com \
  --email admin@example.com \
  --agree-tos \
  --non-interactive
```

**预期输出（成功）：**

```
Saving debug log to /var/log/letsencrypt/letsencrypt.log
Plugins selected: Authenticator standalone, Installer None
Requesting a certificate for example.com and www.example.com
Performing the following challenges: http-01 for each domain
Waiting for verification...
Cleaning up challenges
Successfully received certificate.
Certificate is saved at:
  /etc/letsencrypt/live/example.com/fullchain.pem
Key is saved at:
  /etc/letsencrypt/live/example.com/privkey.pem
This certificate expires on 2026-07-29.
These files will be updated when the certificate renews.
```

#### 验证生成的文件

```bash
# 列出证书文件
sudo ls -la /etc/letsencrypt/live/example.com/

# 预期输出
lrwxrwxrwx 1 root root   41 ... cert.pem -> ../../archive/example.com/cert1.pem
lrwxrwxrwx 1 root root   42 ... chain.pem -> ../../archive/example.com/chain1.pem
lrwxrwxrwx 1 root root   46 ... fullchain.pem -> ../../archive/example.com/fullchain1.pem
lrwxrwxrwx 1 root root   44 ... privkey.pem -> ../../archive/example.com/privkey1.pem

# 使用 openssl 检查证书内容
sudo openssl x509 -in /etc/letsencrypt/live/example.com/fullchain.pem -text -noout | head -20
```

**关键文件说明：**

| 文件 | 用途 | 权限要求 |
|------|------|----------|
| `fullchain.pem` | 服务器证书 + 中间 CA 链（Nginx 用此文件） | 644 或 640 |
| `privkey.pem` | 私钥（**绝不可公开**） | 600 或 400 |
| `chain.pem` | 仅中间 CA 链（Apache 可能需要额外指定） | 644 |
| `cert.pem` | 仅服务器证书（不含中间链） | 644 |

### 1.3 Let's Encrypt — DNS-01 验证（通配符证书）

适用于：无法开放 80 端口、需要 `*.example.com` 通配符证书、或使用 NAT/内网穿透的场景。

#### 手动 DNS TXT 记录方式

```bash
sudo certbot certonly --manual \
  --domain example.com \
  --domain *.example.com \
  --preferred-challenges dns
```

Certbot 会提示添加如下 TXT 记录：

```
Please deploy a DNS TXT record under the name:
_acme-challenge.example.com

with the following value:
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
```

**操作步骤：**

1. 登录 DNS 管理面板（阿里云 DNS、Cloudflare、AWS Route53 等）
2. 添加 TXT 记录：
   - **主机记录**: `_acme-challenge`
   - **记录类型**: TXT
   - **记录值**: `xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx`（Certbot 给出的值）
3. 等待 DNS 生效（通常 1-10 分钟）
4. 在终端按回车继续

**预期输出：**

```
Waiting for verification...
Cleaning up challenges
IMPORTANT NOTES:
 - Congratulations! Your certificate and chain have been saved at:
   /etc/letsencrypt/live/example.com/fullchain.pem
```

#### 自动 DNS API 方式（以 Cloudflare 为例）

```bash
# 安装 certbot-dns-cloudflare 插件
sudo pip install certbot-dns-cloudflare

# 创建 API 凭证文件
mkdir -p ~/.secrets/certbot
cat > ~/.secrets/certbot/cloudflare.ini << 'EOF'
# Cloudflare API Token（需有 DNS:Edit 权限）
dns_cloudflare_api_token = YOUR_CLOUDFLARE_API_TOKEN
EOF

chmod 600 ~/.secrets/certbot/cloudflare.ini

# 申请证书（同时申请裸域和通配符）
sudo certbot certonly \
  --dns-cloudflare \
  --dns-cloudflare-credentials ~/.secrets/certbot/cloudflare.ini \
  --domain example.com \
  --domain *.example.com \
  --email admin@example.com \
  --agree-tos \
  --non-interactive
```

**预期输出：**

```
Requesting a certificate for example.com and *.example.com
Waiting for verification...
Cleaning up challenges
Successfully received certificate.
```

### 1.4 自签名证书（开发/测试环境）

**⚠️ 警告**: 自签名证书在生产环境中**绝对不可用**，浏览器会显示"不安全"警告，且无法防范 MITM。

#### 方式 A：单步生成自签名证书（快速测试，推荐）

使用 OpenSSL 一步生成自签名证书，适用于快速测试 127.0.0.1：

```bash
# 创建证书目录
mkdir -p server/certs

# 一步生成自签名证书（RSA 2048, CN=127.0.0.1, 含 SAN）
openssl req -x509 -newkey rsa:2048 \
  -keyout server/certs/server.key \
  -out server/certs/server.crt \
  -days 365 -nodes \
  -subj "/CN=127.0.0.1" \
  -addext "subjectAltName=IP:127.0.0.1"

# 验证证书内容
openssl x509 -in server/certs/server.crt -text -noout | grep -A1 "Subject Alternative Name"
```

**预期输出（SAN 验证）：**
```
X509v3 Subject Alternative Name:
    IP Address:127.0.0.1
```

**⚠️ 重要**: 使用 `-addext` 参数（OpenSSL ≥ 1.1.1）直接在 `req` 命令中指定 SAN，
无需额外配置文件。如果证书不包含 `subjectAltName`，Chrome/Schannel 会拒绝连接。

#### 方式 B：使用 CA 签发（适用于多域名/IP）

```bash
# 1. 生成 CA 根证书
openssl genrsa -out ca.key 4096
openssl req -x509 -new -nodes \
  -key ca.key \
  -sha256 -days 3650 \
  -out ca.crt \
  -subj "/C=CN/ST=Beijing/L=Beijing/O=ChronoShift Dev/CN=ChronoShift Dev CA"

# 2. 生成服务器私钥
openssl genrsa -out server.key 2048

# 3. 生成证书签名请求 (CSR)
openssl req -new \
  -key server.key \
  -out server.csr \
  -subj "/C=CN/ST=Beijing/L=Beijing/O=ChronoShift/CN=example.com"

# 4. 创建配置文件以避免 SAN 缺失
cat > server.ext << 'EOF'
authorityKeyIdentifier=keyid,issuer
basicConstraints=CA:FALSE
keyUsage=digitalSignature,nonRepudiation,keyEncipherment,dataEncipherment
extendedKeyUsage=serverAuth
subjectAltName=@alt_names

[alt_names]
DNS.1 = example.com
DNS.2 = *.example.com
DNS.3 = localhost
IP.1 = 192.0.2.1
IP.2 = 127.0.0.1
EOF

# 5. 使用 CA 签发服务器证书
openssl x509 -req \
  -in server.csr \
  -CA ca.crt \
  -CAkey ca.key \
  -CAcreateserial \
  -out server.crt \
  -days 365 \
  -sha256 \
  -extfile server.ext

# 6. 验证生成的证书
openssl x509 -in server.crt -text -noout | grep -A1 "Subject Alternative Name"
```

**预期输出（SAN 验证）：**
```
X509v3 Subject Alternative Name:
    DNS:example.com, DNS:*.example.com, DNS:localhost, IP Address:192.0.2.1, IP Address:127.0.0.1
```

**生成的文件清单（方式 B）：**

```
ca.crt          # CA 根证书（需要安装到客户端信任库）
ca.key          # CA 私钥（保护好！）
server.crt      # 服务器证书
server.key      # 服务器私钥
server.csr      # 证书签名请求（签发后可删除）
```

### 1.5 商业 CA 证书申请

以 DigiCert、GlobalSign、阿里云 SSL 证书为例的通用步骤：

```bash
# 1. 生成私钥和 CSR
openssl genrsa -out example.com.key 2048
openssl req -new -key example.com.key -out example.com.csr \
  -subj "/C=CN/ST=Beijing/L=Beijing/O=Your Company/CN=example.com"

# 2. 查看 CSR 内容确认无误
openssl req -in example.com.csr -text -noout | head -15

# 3. 将 example.com.csr 内容提交给 CA 购买证书
# 4. CA 会要求验证域名（通常通过 DNS TXT 记录或邮箱）
# 5. 收到 CA 下发的证书文件包，通常包含：
#    - example.com.crt      （服务器证书）
#    - example.com.ca-bundle（中间 CA 链）
#    - example.com.fullchain（完整链 = 服务器证书 + 中间 CA 链）

# 6. 合并为 fullchain（如需）
cat example.com.crt example.com.ca-bundle > fullchain.pem
```

---

## 第二阶段：服务器端配置

### 2.1 私钥安全保护

```bash
# 设置私钥权限（仅 root 可读）
sudo chmod 600 /etc/letsencrypt/live/example.com/privkey.pem
sudo chown root:root /etc/letsencrypt/live/example.com/privkey.pem

# 验证权限
ls -la /etc/letsencrypt/live/example.com/privkey.pem
# 预期: -rw------- 1 root root ... privkey.pem

# 可选：使用密码加密私钥（每次重启需手动输入密码）
openssl rsa -aes256 -in /etc/letsencrypt/live/example.com/privkey.pem \
  -out /etc/letsencrypt/live/example.com/privkey.enc.pem
# ⚠️ 密码加密后，服务器重启时需要输入密码，不适合自动重启场景
```

### 2.2 Nginx 配置

#### 完整 HTTPS 虚拟主机配置

创建 `/etc/nginx/sites-available/example.com.conf`：

```nginx
server {
    # HTTP → HTTPS 重定向
    listen 80;
    listen [::]:80;
    server_name example.com www.example.com;

    # 301 永久重定向到 HTTPS
    return 301 https://$host$request_uri;

    # 可选：Let's Encrypt 验证文件路径（配合 certbot 自动续期）
    location /.well-known/acme-challenge/ {
        root /var/www/html;
    }
}

server {
    listen 443 ssl http2;
    listen [::]:443 ssl http2;
    server_name example.com www.example.com;

    # ========== 证书配置 ==========
    ssl_certificate     /etc/letsencrypt/live/example.com/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/example.com/privkey.pem;

    # ========== TLS 协议与密码套件 ==========
    ssl_protocols             TLSv1.2 TLSv1.3;
    ssl_ciphers               ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES256-GCM-SHA384:ECDHE-RSA-AES256-GCM-SHA384:ECDHE-ECDSA-CHACHA20-POLY1305:ECDHE-RSA-CHACHA20-POLY1305:DHE-RSA-AES128-GCM-SHA256:DHE-RSA-AES256-GCM-SHA384;
    ssl_prefer_server_ciphers on;
    ssl_ecdh_curve            secp384r1:X25519:prime256v1;

    # ========== 会话缓存与票据 ==========
    ssl_session_cache    shared:SSL:10m;
    ssl_session_timeout  1d;
    ssl_session_tickets  off;

    # ========== HSTS（HTTP Strict Transport Security）==========
    # 首次配置建议用 max-age=3600 测试，稳定后改为 31536000（1 年）
    add_header Strict-Transport-Security "max-age=31536000; includeSubDomains; preload" always;

    # ========== 其他安全标头 ==========
    add_header X-Content-Type-Options    "nosniff" always;
    add_header X-Frame-Options           "DENY" always;
    add_header X-XSS-Protection          "1; mode=block" always;
    add_header Referrer-Policy           "strict-origin-when-cross-origin" always;

    # ========== OCSP Stapling ==========
    ssl_stapling       on;
    ssl_stapling_verify on;
    resolver           8.8.8.8 1.1.1.1 valid=300s;
    resolver_timeout   5s;

    # ========== 反向代理到 Chrono-shift 后端 ==========
    location / {
        proxy_pass http://127.0.0.1:8080;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;

        # WebSocket 支持
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection "upgrade";
    }

    # ========== API 专用代理 ==========
    location /api/ {
        proxy_pass http://127.0.0.1:8080;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
    }

    # ========== 静态文件 ==========
    location /static/ {
        root /var/www/chrono-shift;
        expires 30d;
        add_header Cache-Control "public, immutable";
    }

    # ========== 访问与错误日志 ==========
    access_log /var/log/nginx/example.com.access.log;
    error_log  /var/log/nginx/example.com.error.log;
}
```

#### 验证 Nginx 配置并重启

```bash
# 检查配置文件语法
sudo nginx -t

# 预期输出（成功）：
# nginx: the configuration file /etc/nginx/nginx.conf syntax is ok
# nginx: configuration file /etc/nginx/nginx.conf test is successful

# 重载配置（不中断连接）
sudo systemctl reload nginx

# 或重启服务
sudo systemctl restart nginx

# 确认服务状态
sudo systemctl status nginx --no-pager
```

**预期输出（status）：**

```
● nginx.service - A high performance web server and a reverse proxy server
   Loaded: loaded (/lib/systemd/system/nginx.service; enabled; ...)
   Active: active (running) since ...
   Main PID: 12345 (nginx)
   ...
```

### 2.3 Apache 配置

#### 启用必要模块

```bash
sudo a2enmod ssl
sudo a2enmod headers
sudo a2enmod rewrite
sudo systemctl restart apache2
```

#### 完整 HTTPS 虚拟主机配置

创建 `/etc/apache2/sites-available/example.com-ssl.conf`：

```apache
<VirtualHost *:80>
    ServerName example.com
    ServerAlias www.example.com

    # HTTP → HTTPS 重定向
    Redirect permanent / https://example.com/

    # Let's Encrypt 验证路径
    <Location /.well-known/acme-challenge/>
        Require all granted
    </Location>
</VirtualHost>

<VirtualHost *:443>
    ServerName example.com
    ServerAlias www.example.com

    # ========== 证书配置 ==========
    SSLEngine on
    SSLCertificateFile      /etc/letsencrypt/live/example.com/fullchain.pem
    SSLCertificateKeyFile   /etc/letsencrypt/live/example.com/privkey.pem
    # Apache 2.4.8+ 会自动从 fullchain.pem 读取链，以下为旧版兼容
    # SSLCertificateChainFile /etc/letsencrypt/live/example.com/chain.pem

    # ========== TLS 协议与密码 ==========
    SSLProtocol             all -SSLv3 -TLSv1 -TLSv1.1
    SSLCipherSuite          ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES256-GCM-SHA384:ECDHE-RSA-AES256-GCM-SHA384:ECDHE-ECDSA-CHACHA20-POLY1305:ECDHE-RSA-CHACHA20-POLY1305:DHE-RSA-AES128-GCM-SHA256:DHE-RSA-AES256-GCM-SHA384
    SSLHonorCipherOrder     on

    # ========== 会话缓存 ==========
    SSLSessionCache         shmcb:/var/run/apache2/ssl_scache(512000)
    SSLSessionCacheTimeout  300

    # ========== HSTS ==========
    Header always set Strict-Transport-Security "max-age=31536000; includeSubDomains; preload"

    # ========== 其他安全标头 ==========
    Header always set X-Content-Type-Options "nosniff"
    Header always set X-Frame-Options "DENY"
    Header always set X-XSS-Protection "1; mode=block"

    # ========== OCSP Stapling ==========
    SSLUseStapling          on
    SSLStaplingResponderTimeout 5
    SSLStaplingReturnResponderErrors off
    SSLStaplingCache        shmcb:/var/run/apache2/ssl_stapling_cache(128000)

    # ========== 反向代理到 Chrono-shift 后端 ==========
    ProxyPreserveHost On
    ProxyPass / http://127.0.0.1:8080/
    ProxyPassReverse / http://127.0.0.1:8080/

    # WebSocket 代理
    ProxyPass /ws/ ws://127.0.0.1:8080/ws/

    # ========== 日志 ==========
    ErrorLog  ${APACHE_LOG_DIR}/example.com.error.log
    CustomLog ${APACHE_LOG_DIR}/example.com.access.log combined
</VirtualHost>
```

#### 启用站点并重启

```bash
# 启用站点
sudo a2ensite example.com-ssl.conf

# 禁用默认 HTTP 站点
sudo a2dissite 000-default.conf

# 检查配置语法
sudo apache2ctl configtest

# 预期输出：
# Syntax OK

# 重载 Apache
sudo systemctl reload apache2

# 或重启
sudo systemctl restart apache2

# 确认状态
sudo systemctl status apache2 --no-pager
```

### 2.4 原生 C 服务器 TLS 支持（✅ 已实现）

Chrono-shift 已内置完整的 TLS/HTTPS 支持，无需额外代理或反向代理即可直接启用加密通信。

#### 实现架构

```
客户端 (curl/browser/CLI)
    │  HTTPS (TLSv1.2/TLSv1.3)
    ▼
┌─────────────────────────────────────┐
│   HTTP 服务器 (http_server.c)         │
│  事件驱动 (epoll/WSAPoll) + TLS      │
│  ├── accept() → set_blocking()       │  ← Windows 必须显式设为阻塞
│  ├── tls_server_wrap(fd)             │  ← 接受新连接时自动 TLS 握手
│  │   └── SSL_accept() (阻塞模式)      │  ← TLS 握手需要阻塞 socket
│  ├── set_nonblocking()               │  ← 握手完成后切回非阻塞
│  ├── tls_read(ssl, buf, len)         │  ← 替代 recv()
│  └── tls_write(ssl, buf, len)        │  ← 替代 send()
├─────────────────────────────────────┤
│   TLS 封装层 (tls_server.c)           │
│   OpenSSL (libssl + libcrypto)       │
│   最低 TLSv1.2, 最高 TLSv1.3         │
│   强密码套件优先                      │
│   TLS 1.3 密码套件单独配置            │
└─────────────────────────────────────┘
```

#### 关键实现细节

**1. Windows 非阻塞 socket 继承问题**（⚠️ 重要）

在 Windows (Winsock) 上，从非阻塞监听 socket `accept()` 产生的子 socket **继承非阻塞模式**。
而 OpenSSL 的 `SSL_accept()` 默认需要在**阻塞模式**下工作。如果直接在非阻塞 socket 上调用
`SSL_accept()`，会立即返回 -1，错误码为 `SSL_ERROR_SYSCALL` (1)。

**解决方案**（在 [`server/src/tls_server.c`](../server/src/tls_server.c) 的 `tls_server_wrap()` 中实现）：

```c
// 第 1 步：显式设为阻塞模式（兼容 Windows socket 继承行为）
set_blocking((socket_t)fd);

// 第 2 步：执行 TLS 握手（必须在阻塞模式下）
int ret = SSL_accept(ssl);

// 第 3 步：握手完成后，在 handle_accept() 中切回非阻塞
// set_nonblocking(client_fd); ← 在 http_server.c 中完成
```

**`set_blocking()` 实现**（在 [`server/include/platform_compat.h`](../server/include/platform_compat.h) 中）：

```c
static inline int set_blocking(socket_t fd)
{
#ifdef PLATFORM_WINDOWS
    u_long mode = 0;
    return ioctlsocket(fd, FIONBIO, &mode) == 0 ? 0 : -1;
#else
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
#endif
}
```

**2. TLS 1.3 密码套件配置**

OpenSSL 3.x 要求使用 **`SSL_CTX_set_ciphersuites()`** 单独配置 TLS 1.3 密码套件
（`SSL_CTX_set_cipher_list()` 仅影响 TLS 1.2 及以下版本）：

```c
// TLS 1.2 及以下密码套件
SSL_CTX_set_cipher_list(ctx, "ECDHE+AESGCM:CHACHA20-POLY1305:!aNULL:!eNULL:!LOW:!MD5:!EXP:!RC4:!DES:!3DES");

// TLS 1.3 密码套件（必须单独设置）
SSL_CTX_set_ciphersuites(ctx,
    "TLS_AES_128_GCM_SHA256:"
    "TLS_AES_256_GCM_SHA384:"
    "TLS_CHACHA20_POLY1305_SHA256");
```

**3. 协议版本配置**

使用现代 API（优先于 `SSL_OP_NO_*` 标志）：

```c
SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);
SSL_CTX_set_max_proto_version(ctx, TLS1_3_VERSION);
```

#### 核心文件

| 文件 | 说明 |
|------|------|
| [`server/include/tls_server.h`](../server/include/tls_server.h) | TLS 抽象接口 — 服务端/客户端统一 API |
| [`server/src/tls_server.c`](../server/src/tls_server.c) | OpenSSL 封装实现（~400 行） |
| [`server/src/main.c`](../server/src/main.c) | `--tls-cert` / `--tls-key` CLI 参数解析 |

#### CLI 用法

```bash
# HTTP 模式（默认，不需 OpenSSL）
./build/chrono-server --port 8080

# HTTPS 模式
./build/chrono-server --port 8080 \
  --tls-cert /etc/letsencrypt/live/example.com/fullchain.pem \
  --tls-key  /etc/letsencrypt/live/example.com/privkey.pem
```

> **注意**: TLS 是**完全可选**的。不指定 `--tls-cert` 则运行纯 HTTP 模式，`tls_server_is_enabled()` 返回 false。

#### 依赖安装

```bash
# Ubuntu/Debian
sudo apt install libssl-dev

# CentOS/RHEL
sudo dnf install openssl-devel

# Windows (MinGW)
# 从 https://winlibs.com/ 或通过 MSYS2 获取 OpenSSL
# pacman -S mingw-w64-x86_64-openssl
```

#### 编译与链接

```bash
# Linux — 含 TLS
gcc -std=c99 -Wall -Iinclude \
  src/main.c src/http_server.c src/tls_server.c src/websocket.c \
  src/database.c src/user_handler.c src/message_handler.c \
  src/community_handler.c src/file_handler.c src/utils.c \
  src/json_parser.c src/protocol.c \
  -o build/chrono-server -lpthread -lm -lssl -lcrypto

# Windows (MinGW) — 含 TLS
gcc -std=c99 -Wall -Iinclude \
  src/main.c src/http_server.c src/tls_server.c src/websocket.c \
  src/database.c src/user_handler.c src/message_handler.c \
  src/community_handler.c src/file_handler.c src/utils.c \
  src/json_parser.c src/protocol.c src/rust_stubs.c \
  -o build/chrono-server.exe -lws2_32 -lssl -lcrypto

# Linux — 无 TLS（不需要 OpenSSL）
gcc -std=c99 -Wall -Iinclude \
  src/main.c src/http_server.c src/websocket.c \
  src/database.c ... \
  -o build/chrono-server -lpthread -lm
```

#### TLS 安全配置

| 配置项 | 值 |
|--------|-----|
| 最低 TLS 版本 | TLSv1.2 |
| 最高 TLS 版本 | TLSv1.3 |
| 禁用协议 | SSLv2, SSLv3, TLSv1.0, TLSv1.1 |
| TLS 1.2 密码套件 | `ECDHE+AESGCM:CHACHA20-POLY1305:!aNULL:!eNULL:!LOW:!MD5:!EXP:!RC4:!DES:!3DES` |
| TLS 1.3 密码套件 | `TLS_AES_128_GCM_SHA256:TLS_AES_256_GCM_SHA384:TLS_CHACHA20_POLY1305_SHA256` |
| 服务端密码优先 | 是 (`SSL_OP_CIPHER_SERVER_PREFERENCE`) |
| 会话缓存 | OpenSSL 内置 |
| 客户端验证 | 可选（非 mTLS，默认不要求客户端证书） |

#### API 说明

**服务端 API**（[`server/include/tls_server.h`](../server/include/tls_server.h)）:

| 函数 | 说明 |
|------|------|
| `tls_server_init(cert, key)` | 初始化全局 TLS 上下文，加载证书和私钥 |
| `tls_server_wrap(fd)` | 将已接受的 socket 包装为 TLS 连接，执行握手 |
| `tls_server_cleanup()` | 释放全局 TLS 上下文 |
| `tls_server_is_enabled()` | 检查 TLS 是否已启用 |
| `tls_read(ssl, buf, len)` | 读取解密数据（非阻塞返回 0 + EAGAIN） |
| `tls_write(ssl, buf, len)` | 写入加密数据（非阻塞返回 0 + EAGAIN） |
| `tls_close(ssl)` | 关闭 TLS 连接（发送 close_notify） |
| `tls_get_info(ssl, buf, size)` | 获取连接信息（协议版本、密码套件、SNI） |

**客户端 API**（[`server/include/tls_server.h`](../server/include/tls_server.h)）:

| 函数 | 说明 |
|------|------|
| `tls_client_init(ca_file)` | 初始化客户端 TLS 上下文（NULL=系统信任库） |
| `tls_client_connect(&ssl, host, port)` | 建立 TLS 连接到指定主机（含 SNI） |
| `tls_client_cleanup()` | 释放客户端全局上下文 |

### 2.5 CLI 调试工具 HTTPS 支持（✅ 已实现）

[`server/tools/debug_cli.c`](../server/tools/debug_cli.c) 已支持通过 `connect <host> <port> tls` 命令进行 HTTPS 调试：

```bash
# 编译（含 TLS）
gcc -std=c99 -Wall -Iinclude tools/debug_cli.c src/tls_server.c \
  -o build/debug_cli -lws2_32 -lssl -lcrypto

# 连接 HTTPS 服务器
connect example.com 443 tls

# 或通过环境变量启用 TLS
set CHRONO_TLS=1    # Windows
export CHRONO_TLS=1 # Linux
connect example.com 443
```

连接成功后会打印 TLS 版本和密码套件信息。

### 2.6 客户端 TLS 支持（✅ 已实现）

[`client/src/network.c`](../client/src/network.c) 和 [`client/include/network.h`](../client/include/network.h) 已集成客户端 TLS：

```c
NetworkContext ctx;
net_init(&ctx);

// 启用 TLS
net_set_tls(&ctx, true);

// 连接（自动执行 TLS 握手）
net_connect(&ctx, "example.com", 443);

// 后续的 net_http_request(), net_ws_connect() 等
// 自动使用加密通信
net_http_request(&ctx, "GET", "/api/health", NULL, NULL);

// 断开（自动发送 close_notify）
net_disconnect(&ctx);
```

### 2.7 自定义 Rust 服务器配置（使用 rustls）

```rust
// 在 Cargo.toml 中添加依赖
// [dependencies]
// rustls = "0.23"
// rustls-pemfile = "2"
// tokio-rustls = "0.26"
// tokio = { version = "1", features = ["full"] }

use std::fs::File;
use std::io::BufReader;
use std::sync::Arc;
use rustls::{ServerConfig, Certificate, PrivateKey};

/// 创建 rustls TLS 服务器配置
pub fn create_tls_config() -> Result<ServerConfig, Box<dyn std::error::Error>> {
    // 加载证书链
    let cert_file = File::open("/etc/letsencrypt/live/example.com/fullchain.pem")?;
    let mut cert_reader = BufReader::new(cert_file);
    let certs: Vec<Certificate> = rustls_pemfile::certs(&mut cert_reader)?
        .into_iter()
        .map(Certificate)
        .collect();

    if certs.is_empty() {
        return Err("No certificates found".into());
    }

    // 加载私钥
    let key_file = File::open("/etc/letsencrypt/live/example.com/privkey.pem")?;
    let mut key_reader = BufReader::new(key_file);
    let keys: Vec<PrivateKey> = rustls_pemfile::rsa_private_keys(&mut key_reader)?
        .into_iter()
        .map(PrivateKey)
        .collect();

    if keys.is_empty() {
        return Err("No private keys found".into());
    }

    // 配置 TLS
    let config = ServerConfig::builder()
        .with_safe_default_cipher_suites()
        .with_safe_default_kx_groups()
        .with_protocol_versions(&[&rustls::version::TLS12, &rustls::version::TLS13])?
        .with_no_client_auth()
        .with_single_cert(certs, keys.remove(0))?;

    Ok(config)
}
```

---

## 第三阶段：客户端信任配置

### 3.1 公共 CA（Let's Encrypt / 商业 CA）

**无需额外操作**，前提是客户端操作系统信任库保持更新：

```bash
# 验证信任库是否包含 Let's Encrypt 根证书
# Linux
grep -r "Let's Encrypt" /usr/share/ca-certificates/ 2>/dev/null || \
  openssl crl2pkcs7 -nocrl -certfile /etc/ssl/certs/ca-certificates.crt 2>/dev/null | \
  openssl pkcs7 -print_certs 2>/dev/null | grep "Let's Encrypt"

# Windows（PowerShell）
Get-ChildItem Cert:\LocalMachine\CA | Where-Object { $_.Subject -like "*Let's Encrypt*" }

# macOS
security find-certificate -c "Let's Encrypt" /System/Library/Keychains/SystemRootCertificates.keychain
```

**预期输出（Linux）：** 显示 ISRG Root X1 等根证书信息。

如果不存在，更新 CA 证书包：

```bash
# Ubuntu/Debian
sudo apt update && sudo apt install ca-certificates -y

# CentOS/RHEL
sudo yum update ca-certificates -y
```

### 3.2 自签名证书 — 安装到系统信任库

#### Windows

**方法 A：使用 certmgr.msc（图形界面）**

1. 按 `Win+R`，输入 `certmgr.msc`，回车
2. 展开"受信任的根证书颁发机构" → "证书"
3. 右键 → "所有任务" → "导入"
4. 浏览选择 `ca.crt` 文件
5. 选择"将所有的证书放入下列存储" → "受信任的根证书颁发机构"
6. 完成 → 确认安全警告 → 是

**方法 B：使用命令行（管理员权限）**

```cmd
:: 安装到"受信任的根证书颁发机构"
certutil -addstore Root ca.crt

:: 验证安装
certutil -store Root | findstr "ChronoShift"

:: 预期输出（部分）：
:: ================ Certificate X ================
:: Issuer: CN=ChronoShift Dev CA
:: Subject: CN=ChronoShift Dev CA
```

#### Linux（Ubuntu/Debian）

```bash
# 1. 复制 CA 证书到系统 CA 目录
sudo cp ca.crt /usr/local/share/ca-certificates/chrono-shift-ca.crt

# 2. 更新 CA 证书数据库
sudo update-ca-certificates

# 预期输出：
# Updating certificates in /etc/ssl/certs...
# 1 added, 0 removed; done.
# Running hooks in /etc/ca-certificates/update.d...
# done.

# 3. 验证安装
openssl verify -CAfile /etc/ssl/certs/ca-certificates.crt server.crt

# 预期输出：
# server.crt: OK
```

#### Linux（CentOS/RHEL）

```bash
# 1. 复制 CA 证书
sudo cp ca.crt /etc/pki/ca-trust/source/anchors/chrono-shift-ca.crt

# 2. 更新 CA 证书数据库
sudo update-ca-trust

# 3. 验证
openssl verify -CAfile /etc/pki/tls/certs/ca-bundle.crt server.crt
```

#### macOS

```bash
# 安装 CA 证书到系统信任库
sudo security add-trusted-cert -d -r trustRoot \
  -k /Library/Keychains/System.keychain ca.crt

# 验证安装
security find-certificate -c "ChronoShift Dev CA"

# 预期输出显示证书详细信息
```

### 3.3 编程客户端配置

#### cURL

```bash
# 使用系统信任库（公网 CA 证书或已安装的自签名 CA）
curl -v https://example.com/api/health

# 使用自定义 CA 证书（不依赖系统信任库）
curl --cacert /path/to/ca.crt https://example.com/api/health

# 跳过证书验证（仅测试用，不安全！）
curl --insecure -v https://example.com/api/health
```

#### Python requests

```python
import requests

# 使用系统信任库（默认）
response = requests.get("https://example.com/api/health")
print(response.status_code)

# 使用自定义 CA 证书
response = requests.get(
    "https://example.com/api/health",
    verify="/path/to/ca.crt"
)

# 跳过验证（仅测试用，不安全！）
response = requests.get(
    "https://example.com/api/health",
    verify=False  # 会产生 InsecureRequestWarning
)
# 抑制警告：
import urllib3
urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)
```

#### Node.js

```javascript
const https = require('https');
const fs = require('fs');

// 使用自定义 CA
const options = {
  hostname: 'example.com',
  port: 443,
  path: '/api/health',
  ca: fs.readFileSync('/path/to/ca.crt')
};

https.get(options, (res) => {
  console.log('Status:', res.statusCode);
});

// 跳过验证（NODE_TLS_REJECT_UNAUTHORIZED=0，仅测试用）
// 设置环境变量: NODE_TLS_REJECT_UNAUTHORIZED=0
```

#### Java

```bash
# 导入 CA 证书到 Java 信任库
keytool -import -trustcacerts \
  -keystore $JAVA_HOME/lib/security/cacerts \
  -storepass changeit \
  -alias chrono-shift-ca \
  -file /path/to/ca.crt

# 验证导入
keytool -list -keystore $JAVA_HOME/lib/security/cacerts \
  -storepass changeit | grep chrono-shift
```

#### Go

```go
package main

import (
    "crypto/tls"
    "crypto/x509"
    "io/ioutil"
    "net/http"
)

func main() {
    // 加载自定义 CA
    caCert, _ := ioutil.ReadFile("/path/to/ca.crt")
    caCertPool := x509.NewCertPool()
    caCertPool.AppendCertsFromPEM(caCert)

    client := &http.Client{
        Transport: &http.Transport{
            TLSClientConfig: &tls.Config{
                RootCAs: caCertPool,
            },
        },
    }

    resp, _ := client.Get("https://example.com/api/health")
    defer resp.Body.Close()
}
```

### 3.4 关于双向 TLS（mTLS）的简要说明

**本指南不涉及 mTLS 配置。** 双向 TLS 要求：

- 服务端配置 `ssl_client_certificate`（Nginx）或 `SSLVerifyClient require`（Apache）
- 客户端持有客户端证书和私钥
- 仅在需要服务端验证客户端身份的高安全性场景使用（如微服务间通信）

---

## 第四阶段：端到端测试与验证

### 4.1 测试前置条件

```bash
# 确保服务器 HTTPS 端口可达
curl -o /dev/null -s -w "%{http_code}\n" https://example.com
# 预期输出: 200
```

### 4.2 证书链验证

```bash
openssl s_client -connect example.com:443 -showcerts
```

**预期输出（成功）：**

```
CONNECTED(00000005)
depth=2 C=US, O=Internet Security Research Group, CN=ISRG Root X1
verify return:1
depth=1 C=US, O=Let's Encrypt, CN=R10
verify return:1
depth=0 CN=example.com
verify return:1
---
Certificate chain
 0 s:CN=example.com                          # ← 服务器证书
   i:C=US, O=Let's Encrypt, CN=R10           # ← 由中间 CA 签发
   a:PKEY: id-ecPublicKey, 256 (bit)
   -----BEGIN CERTIFICATE-----
   ...（base64 编码的证书内容）
   -----END CERTIFICATE-----
 1 s:C=US, O=Let's Encrypt, CN=R10           # ← 中间 CA 证书
   i:C=US, O=Internet Security Research Group, CN=ISRG Root X1  # ← 由根 CA 签发
   -----BEGIN CERTIFICATE-----
   ...
   -----END CERTIFICATE-----
---
Server certificate
subject=CN=example.com
issuer=C=US, O=Let's Encrypt, CN=R10
---
No client certificate CA names sent
---
SSL handshake has read 3121 bytes and written 389 bytes
Verification: OK                       # ← 验证通过！
---
```

**关键检查点：**

| 检查项 | 命令 | 成功标志 |
|--------|------|----------|
| 证书链完整性 | 查看 `Certificate chain` 段 | 至少包含服务器证书 + 中间 CA 证书 |
| 验证状态 | 查找 `Verification:` 行 | `Verification: OK` |
| 证书主题 | `subject=` 行 | `CN=example.com` 匹配访问域名 |
| 证书颁发者 | `issuer=` 行 | 指向已知 CA |
| 有效日期 | `openssl x509 -in` 查看 | 在 valid 范围内 |

**常见问题与修复：**

```
# 问题：证书链不完整（仅返回服务器证书，无中间 CA）
verify error:num=20:unable to get local issuer certificate

# 解决方案：确保 Nginx/Apache 使用 fullchain.pem 而非 cert.pem
# 验证 fullchain.pem 包含两个证书
openssl crl2pkcs7 -nocrl -certfile /etc/letsencrypt/live/example.com/fullchain.pem | \
  openssl pkcs7 -print_certs | grep subject
# 应输出两行：服务器证书 + 中间 CA
```

```
# 问题：主机名不匹配
verify error:num=61:hostname mismatch

# 解决方案：
# 1. 确认访问域名与证书 CN/SAN 匹配
openssl x509 -in fullchain.pem -text -noout | grep "Subject Alternative Name"
# 2. 如果使用 IP 访问，证书必须包含 IP SAN
```

```
# 问题：证书已过期
verify error:num=10:certificate has expired

# 解决方案：
# 查看有效期
openssl x509 -in fullchain.pem -dates -noout
# 续期（Let's Encrypt）
sudo certbot renew
```

### 4.3 TLS 握手验证

#### TLS 1.2 握手

```bash
openssl s_client -connect example.com:443 -tls1_2
```

**预期输出片段：**

```
New, TLSv1.2, Cipher is ECDHE-RSA-AES128-GCM-SHA256
Server public key is 2048 bit
Secure Renegotiation IS supported
Compression: NONE
Expansion: NONE
No ALPN negotiated
SSL-Session:
    Protocol  : TLSv1.2
    Cipher    : ECDHE-RSA-AES128-GCM-SHA256
```

#### TLS 1.3 握手

```bash
openssl s_client -connect example.com:443 -tls1_3
```

**预期输出片段：**

```
New, TLSv1.3, Cipher is TLS_AES_256_GCM_SHA384
Server public key is 2048 bit
SSL-Session:
    Protocol  : TLSv1.3
    Cipher    : TLS_AES_256_GCM_SHA384
```

#### 测试弱协议是否被拒绝

```bash
# SSLv3 应被拒绝
openssl s_client -connect example.com:443 -ssl3 2>&1 | grep -E "error|failure"
# 预期: 连接失败

# TLSv1.0 应被拒绝
openssl s_client -connect example.com:443 -tls1 2>&1 | grep -E "error|failure"
# 预期: 连接失败

# TLSv1.1 应被拒绝
openssl s_client -connect example.com:443 -tls1_1 2>&1 | grep -E "error|failure"
# 预期: 连接失败
```

### 4.4 HTTP 重定向测试

```bash
# 测试 HTTP → HTTPS 重定向
curl -v http://example.com/
```

**预期输出：**

```
*   Trying 192.0.2.1:80...
* Connected to example.com (192.0.2.1) port 80 (#0)
> GET / HTTP/1.1
> Host: example.com
> User-Agent: curl/...
> Accept: */*
>
< HTTP/1.1 301 Moved Permanently        # ← 301 重定向
< Location: https://example.com/         # ← 目标为 HTTPS
< Content-Length: 0
< Date: ...
<
* Connection #0 to host example.com left intact
```

```bash
# 测试 HTTPS 正常访问
curl -v https://example.com/
```

**预期输出：**

```
*   Trying 192.0.2.1:443...
* Connected to example.com (192.0.2.1) port 443 (#0)
* SSL connection using TLSv1.3 / TLS_AES_256_GCM_SHA384  # ← TLS 加密连接
* ALPN: server accepted h2
* Server certificate:                                   # ← 证书验证
*  subject: CN=example.com
*  start date: ...
*  expire date: ...
*  subjectAltName: host "example.com" matched cert's "example.com"
*  issuer: C=US; O=Let's Encrypt; CN=R10
*  SSL certificate verify ok.                           # ← 验证通过
> GET / HTTP/1.1
> Host: example.com
> ...
<
< HTTP/1.1 200 OK                                       # ← 正常响应
< ...
```

### 4.5 HSTS 标头验证

```bash
curl -sI https://example.com/ | grep -i strict-transport-security
```

**预期输出：**

```
Strict-Transport-Security: max-age=31536000; includeSubDomains; preload
```

**如果未设置 HSTS：**

```
# 检查 Nginx/Apache 配置是否正确
# Nginx: 确保有 add_header Strict-Transport-Security ...
# Apache: 确保有 Header always set Strict-Transport-Security ...
```

### 4.6 弱密码扫描

#### 使用 nmap

```bash
# 安装 nmap（如未安装）
sudo apt install nmap   # Ubuntu/Debian
sudo dnf install nmap   # CentOS/RHEL

# 扫描 TLS 密码套件
nmap --script ssl-enum-ciphers -p 443 example.com
```

**预期输出（安全配置）：**

```
Starting Nmap 7.80 ( https://nmap.org ) at ...
Nmap scan report for example.com (192.0.2.1)
Host is up (0.042s latency).

PORT    STATE SERVICE
443/tcp open  https
| ssl-enum-ciphers:
|   TLSv1.2:
|     ciphers:
|       TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256 (ecdh_x25519) - A
|       TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384 (ecdh_x25519) - A
|       TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256 (ecdh_x25519) - A
|       TLS_DHE_RSA_WITH_AES_128_GCM_SHA256 (dh 2048) - A
|       TLS_DHE_RSA_WITH_AES_256_GCM_SHA384 (dh 2048) - A
|     compressors:
|       NULL
|     cipher preference: server
|     warnings:
|       (none)                                    # ← 无警告
|   TLSv1.3:
|     ciphers:
|       TLS_AKE_WITH_AES_128_GCM_SHA256 (ecdh_x25519) - A
|       TLS_AKE_WITH_AES_256_GCM_SHA384 (ecdh_x25519) - A
|       TLS_AKE_WITH_CHACHA20_POLY1305_SHA256 (ecdh_x25519) - A
|     cipher preference: client
|_  least strength: A

Nmap done: 1 IP address (1 host up) scanned in 5.23 seconds
```

**不安全配置的警告示例：**

```
|     warnings:
|       forward secrecy not supported by any cipher  # ← 无前向安全性
|       CBC-mode cipher in TLSv1.2                  # ← 允许 CBC 模式
|      弱密码: DES, 3DES, RC4                     # ← 使用了弱密码
```

**弱密码列表（应禁用）：**

| 密码 | 风险 | 替代方案 |
|------|------|----------|
| `RC4-SHA` / `RC4-MD5` | 已被完全破解 | ECDHE-RSA-AES128-GCM-SHA256 |
| `DES-CBC3-SHA` (3DES) | SWEET32 攻击 | AES-GCM |
| `ECDHE-RSA-DES-CBC3-SHA` | 64位块密码 | CHACHA20-POLY1305 |
| `DHE-RSA-EXPORT` | 出口级弱密码 | 全部禁用 |
| `aNULL` / `eNULL` | 无认证/无加密 | 必须禁用 |

#### 使用 sslyze

```bash
# 安装 sslyze
pip install sslyze

# 执行全面扫描
sslyze --regular example.com:443
```

### 4.7 客户端信任确认

#### 信任 CA 的客户端 → 应成功

```bash
# 自签名 CA 已安装到系统信任库后
curl -v https://example.com/
# 应显示 SSL certificate verify ok.

# 或手动指定 CA
curl --cacert /path/to/ca.crt https://example.com/
# 应显示 SSL certificate verify ok.
```

#### 跳过验证 → 应工作但警告

```bash
curl --insecure -v https://example.com/
```

**预期输出（含警告但请求成功）：**

```
* SSL certificate verify result: self-signed certificate (18), continuing anyway.
* TLSv1.3 (OUT), TLS alert, unknown (628):
> GET / HTTP/1.1
...
< HTTP/1.1 200 OK
```

#### 无 CA 的客户端 → 应失败

```bash
# 在未安装自签名 CA 的机器上执行
curl -v https://example.com/
```

**预期输出（证书错误）：**

```
* SSL certificate problem: self-signed certificate in certificate chain
* Closing connection 0
curl: (60) SSL certificate problem: self-signed certificate in certificate chain
More details here: https://curl.se/docs/sslcerts.html

curl failed to verify the legitimacy of the server and therefore could not
establish a secure connection to it. To learn more about this situation and
how to fix it, please visit the web page mentioned above.
```

---

## 附录：常见错误速查

| 错误消息 | 原因 | 解决方案 |
|----------|------|----------|
| `unable to get local issuer certificate` | 证书链不完整 | 使用 `fullchain.pem` 而非 `cert.pem` |
| `hostname mismatch` | 域名与证书 CN/SAN 不匹配 | 确保证书包含正确的域名或 IP SAN |
| `certificate has expired` | 证书已过期 | 运行 `certbot renew` 续期 |
| `permission denied (publickey)` | 私钥权限过松 | `chmod 600 privkey.pem` |
| `ssl_protocols` 配置无效 | 未允许的协议版本 | 检查 `ssl_protocols TLSv1.2 TLSv1.3;` |
| `no suitable key share` | TLS 1.3 密钥交换参数不匹配 | 配置 `ssl_ecdh_curve` |
| `error:1409442E` | 证书验证失败 | 检查证书是否完整、是否过期 |
| 浏览器显示 NET::ERR_CERT_AUTHORITY_INVALID | 证书颁发机构不受信任 | 安装 CA 到系统信任库 |
| 浏览器显示 NET::ERR_CERT_COMMON_NAME_INVALID | 域名不匹配 | 确保证书 SAN 包含访问域名 |
| `curl: (35) SSL connect error` | TLS 握手失败 | 检查服务器是否正确加载了证书 |
| HSTS 标头缺失 | 未配置 HSTS | 在 server block 中添加 `add_header Strict-Transport-Security ...` |
| `SSLError: [SSL: WRONG_VERSION_NUMBER]` | 端口不是 HTTPS 端口 | 确认连接的是 443 端口 |

---

## 附录：Certbot 自动续期配置

```bash
# 测试续期流程（不实际更新）
sudo certbot renew --dry-run

# 预期输出：
# Congratulations, all renewals succeeded. The following certs have been renewed:
#   /etc/letsencrypt/live/example.com/fullchain.pem (success)

# 查看 systemd 定时器
sudo systemctl status certbot.timer

# 预期输出显示定时器激活状态（每天执行两次）

# 手动触发续期
sudo certbot renew

# 续期后需重载服务器
# Nginx
sudo systemctl reload nginx
# Apache
sudo systemctl reload apache2
```

## 附录：Chrono-shift 快速启用 HTTPS

### 完整操作流程（Windows 开发环境）

#### 1. 生成自签名测试证书

```bash
cd server
mkdir -p certs

openssl req -x509 -newkey rsa:2048 \
  -keyout certs/server.key -out certs/server.crt \
  -days 365 -nodes \
  -subj "/CN=127.0.0.1" \
  -addext "subjectAltName=IP:127.0.0.1"
```

#### 2. 编译含 TLS 的服务器

```bash
cd server
mingw32-make HAS_TLS=1 clean build
```

#### 3. 运行 HTTPS 服务器

```cmd
:: 确保 OpenSSL DLL 在 PATH 中
set PATH=D:\mys32\mingw64\bin;%PATH%

:: 启动 HTTPS 服务器
out\chrono-server.exe --port 8443 --tls-cert certs\server.crt --tls-key certs\server.key
```

**预期启动日志：**
```
[TLS] OpenSSL 版本: OpenSSL 3.6.2 9 Sep 2024
[TLS] 已加载证书: certs/server.crt
[TLS] 已加载私钥: certs/server.key
[TLS] 已启用 (最低: TLSv1.2, 最高: TLSv1.3)
[INFO] 服务器已启动, 监听 0.0.0.0:8443
[TLS]  使用 TLS 桩实现 (无加密, make HAS_TLS=1 启用)
```

#### 4. 验证 TLS 连接

**使用 openssl s_client（推荐，精确验证）：**

```bash
openssl s_client -connect 127.0.0.1:8443
```

**预期输出：**
```
CONNECTED(000001A8)
depth=0 CN=127.0.0.1
verify return:1
---
Certificate chain
 0 s:CN=127.0.0.1
   i:CN=127.0.0.1
   a:PKEY: rsaEncryption, 2048 (bit)
   -----BEGIN CERTIFICATE-----
   ...（证书内容）...
   -----END CERTIFICATE-----
---
Server certificate
subject=CN=127.0.0.1
issuer=CN=127.0.0.1
---
No client certificate CA names sent
Peer signing digest: SHA256
Peer signature type: RSA-PSS
Server Temp Key: X25519, 253 bits
---
SSL handshake has read 2426 bytes and written 1586 bytes
---
New, TLSv1.3, Cipher is TLS_AES_128_GCM_SHA256
SSL-Session:
    Protocol  : TLSv1.3
    Cipher    : TLS_AES_128_GCM_SHA256
    Session-ID: ...
    Session-ID-ctx:
    Resumption PSK: ...
    PSK identity: None
    PSK identity hint: None
    Group: X25519
    ...
    Verify return code: 0 (ok)
---
```

**关键验证点：**
- `New, TLSv1.3, Cipher is TLS_AES_128_GCM_SHA256` — TLS 1.3 握手成功
- `SSL handshake has read 2426 bytes and written 1586 bytes` — 完成双向加密通信
- `Verify return code: 0 (ok)` — 证书验证通过

#### 5. 测试回环流量加密

**测试 1 — HTTP 明文访问被拒绝：**
```bash
curl http://127.0.0.1:8443/
```
预期：`curl: (56) Recv failure: Connection was reset`

**测试 2 — HTTPS 加密通信（使用 Schannel）：**
```bash
curl -k https://127.0.0.1:8443/
```
预期：连接建立成功（`-k` 跳过自签名证书验证）

**测试 3 — 验证无法解密回环流量：**
```bash
# 尝试用 openssl s_server 监听不工作（端口占用）
# 或使用 tcpdump/Wireshark 抓包时，TLS 加密载荷不可读
```

### 使用 Makefile 快速编译

```bash
cd server
mingw32-make HAS_TLS=1 clean build    # 编译服务端（含 TLS）
```

### Windows 运行注意事项

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| 启动时报 `STATUS_DLL_NOT_FOUND` | OpenSSL DLL 不在 PATH | `set PATH=D:\mys32\mingw64\bin;%PATH%` |
| 编译时报 `openssl/ssl.h` 未找到 | MinGW 未安装 OpenSSL 开发包 | `pacman -S mingw-w64-x86_64-openssl` |
| `SSL_accept` 失败（`err=1`） | socket 继承非阻塞模式 | 已修复：`tls_server_wrap()` 调用 `set_blocking()` |
| curl 持续重协商（Schannel） | Windows Schannel 与 OpenSSL 兼容性问题 | 使用 `openssl s_client` 验证；不影响服务器功能 |

### CLI 调试工具测试 HTTPS

```bash
cd server
./build/debug_cli
> connect example.com 443 tls
> GET /api/health
```

---

> **文档维护者**: Chrono-shift 项目组  
> **最后更新**: 2026-04-30  
> **许可证**: 本文档遵循 GPLv3 协议，参见项目 [`LICENSE`](../LICENSE) 文件
