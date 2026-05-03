#!/usr/bin/env bash
# ============================================================
# 墨竹 (Chrono-shift) 自签名 TLS 证书生成脚本 (Linux/macOS)
#
# 用途: 快速生成开发用自签名 RSA 2048 证书
# 用法: ./gen_cert.sh [output_dir] [cn] [days]
#   output_dir  证书输出目录 (默认: ./certs)
#   cn          证书 Common Name (默认: 127.0.0.1)
#   days        证书有效天数 (默认: 3650, 即 10 年)
#
# 输出:
#   output_dir/cert.pem  — 自签名证书 (含 SAN)
#   output_dir/key.pem   — RSA 私钥
#
# 依赖: OpenSSL (系统 PATH 中可用)
# ============================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# --- 颜色 ---
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# --- 参数 ---
OUTPUT_DIR="${1:-./certs}"
CN="${2:-127.0.0.1}"
DAYS="${3:-3650}"

CERT_FILE="${OUTPUT_DIR}/cert.pem"
KEY_FILE="${OUTPUT_DIR}/key.pem"

# --- 检测 OpenSSL ---
if ! command -v openssl &>/dev/null; then
    echo -e "${RED}[ERROR]${NC} 未找到 OpenSSL！"
    echo "请安装 OpenSSL:"
    echo "  - macOS: brew install openssl"
    echo "  - Ubuntu/Debian: sudo apt install openssl"
    echo "  - CentOS/RHEL: sudo yum install openssl"
    exit 1
fi

OPENSSL_CMD=$(command -v openssl)
echo -e "${BLUE}[INFO]${NC} 使用 OpenSSL: ${OPENSSL_CMD}"

# --- 检查是否已存在 ---
if [ -f "$CERT_FILE" ]; then
    echo -e "${YELLOW}[WARN]${NC} 证书文件已存在: ${CERT_FILE}"
    read -r -p "是否覆盖? (y/N): " OVERWRITE
    if [ "$OVERWRITE" != "y" ] && [ "$OVERWRITE" != "Y" ]; then
        echo -e "${BLUE}[INFO]${NC} 已取消"
        exit 0
    fi
fi
if [ -f "$KEY_FILE" ]; then
    echo -e "${YELLOW}[WARN]${NC} 私钥文件已存在: ${KEY_FILE}"
    read -r -p "是否覆盖? (y/N): " OVERWRITE
    if [ "$OVERWRITE" != "y" ] && [ "$OVERWRITE" != "Y" ]; then
        echo -e "${BLUE}[INFO]${NC} 已取消"
        exit 0
    fi
fi

# --- 创建输出目录 ---
mkdir -p "$OUTPUT_DIR"

echo -e "${BLUE}"
echo "============================================"
echo "   墨竹 自签名证书生成工具"
echo "============================================"
echo -e "${NC}"
echo "  输出目录: ${OUTPUT_DIR}"
echo "  Common Name: ${CN}"
echo "  有效期: ${DAYS} 天"
echo "  OpenSSL: ${OPENSSL_CMD}"
echo ""

# --- Step 1: 生成 RSA 私钥 ---
echo -e "${BLUE}[1/3]${NC} 生成 RSA 2048 位私钥..."
openssl genrsa -out "$KEY_FILE" 2048
echo -e "${GREEN}[OK]${NC} 私钥已生成: ${KEY_FILE}"

# --- Step 2: 生成自签名证书 (含 SAN) ---
echo -e "${BLUE}[2/3]${NC} 生成自签名证书 (SHA-256, SAN)..."

SAN_CONFIG=$(mktemp /tmp/chrono_san.XXXXXX.cnf)
cat > "$SAN_CONFIG" <<EOF
[req]
distinguished_name = req_distinguished_name
x509_extensions = v3_req
prompt = no

[req_distinguished_name]
CN = ${CN}

[v3_req]
keyUsage = keyEncipherment, dataEncipherment, digitalSignature
extendedKeyUsage = serverAuth
subjectAltName = @alt_names

[alt_names]
DNS.1 = ${CN}
IP.1 = 127.0.0.1
IP.2 = ::1
EOF

openssl req -x509 -nodes -days "$DAYS" -newkey rsa:2048 \
    -keyout "$KEY_FILE" -out "$CERT_FILE" \
    -config "$SAN_CONFIG" \
    -sha256

rm -f "$SAN_CONFIG"
echo -e "${GREEN}[OK]${NC} 证书已生成: ${CERT_FILE}"

# --- Step 3: 验证证书 ---
echo -e "${BLUE}[3/3]${NC} 验证证书..."
if openssl x509 -in "$CERT_FILE" -text -noout >/dev/null 2>&1; then
    openssl x509 -in "$CERT_FILE" -noout -subject -dates
    echo -e "${GREEN}[OK]${NC} 证书验证通过"
else
    echo -e "${YELLOW}[WARN]${NC} 证书验证失败"
fi

echo ""
echo -e "${GREEN}============================================${NC}"
echo -e "${GREEN}  证书生成完成！${NC}"
echo -e "${GREEN}============================================${NC}"
echo "  证书: ${CERT_FILE}"
echo "  私钥: ${KEY_FILE}"
echo ""
echo "  使用方式:"
echo "    - 客户端: 将 cert.pem 和 key.pem 放在客户端运行目录"
echo "    - CLI:    运行 chrono-devtools gen-cert"
echo "    - 开发:   添加 --ignore-certificate-errors 浏览器参数"
echo ""
