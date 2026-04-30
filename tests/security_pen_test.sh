#!/bin/bash
# ============================================================
# 墨竹 (Chrono-shift) 安全渗透测试脚本
# 测试类别: SQL注入 / XSS / 路径遍历 / JWT伪造 / 权限越界
# ============================================================

REPORT_DIR="reports"
REPORT_FILE="${REPORT_DIR}/security_pen_test_results.md"
HOST="127.0.0.1"
PORT="4443"
BASE_URL="https://${HOST}:${PORT}"
PASS=0
FAIL=0
TOTAL=0

mkdir -p "${REPORT_DIR}"

# ---- 颜色输出 ----
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

info()    { echo -e "${BLUE}[*]${NC} $1"; }
pass()    { echo -e "${GREEN}[PASS]${NC} $1"; PASS=$((PASS+1)); }
fail()    { echo -e "${RED}[FAIL]${NC} $1"; FAIL=$((FAIL+1)); }
warning() { echo -e "${YELLOW}[WARN]${NC} $1"; }

# ---- 测试辅助函数 ----
run_test() {
    local test_name="$1"
    local method="$2"
    local path="$3"
    local body="$4"
    local expected_status_low="$5"
    local expected_status_high="$6"
    local fail_condition="$7"  # 响应内容包含此字符串视为失败

    TOTAL=$((TOTAL+1))
    info "测试 #${TOTAL}: ${test_name}"

    # 构建 curl 命令
    local cmd="curl -s -o \"${REPORT_DIR}/.resp_body\" -w \"%{http_code}\" -X ${method} \"${BASE_URL}${path}\""
    if [ -n "${body}" ]; then
        cmd="${cmd} -H \"Content-Type: application/json\" -d '${body}'"
    fi

    local http_code=$(eval ${cmd})
    local resp_body=$(cat "${REPORT_DIR}/.resp_body" 2>/dev/null)

    # 判断结果
    local failed=0
    if [ "${http_code}" -lt "${expected_status_low}" ] || [ "${http_code}" -gt "${expected_status_high}" ]; then
        warning "  HTTP ${http_code} (期望 ${expected_status_low}-${expected_status_high})"
        failed=1
    fi

    if [ -n "${fail_condition}" ] && echo "${resp_body}" | grep -qi "${fail_condition}"; then
        warning "  响应包含不安全内容: ${fail_condition}"
        failed=1
    fi

    if [ "${failed}" -eq 1 ]; then
        fail "${test_name} (HTTP ${http_code})"
        echo "    响应: ${resp_body:0:200}" >> "${REPORT_DIR}/.failures"
    else
        pass "${test_name} (HTTP ${http_code})"
    fi

    # 记录详细结果
    {
        echo "### #${TOTAL}: ${test_name}"
        echo "- **方法**: ${method}"
        echo "- **路径**: ${path}"
        if [ -n "${body}" ]; then echo "- **请求体**: \`${body}\`"; fi
        echo "- **HTTP 状态码**: ${http_code}"
        echo "- **期望范围**: ${expected_status_low}-${expected_status_high}"
        if [ "${failed}" -eq 1 ]; then
            echo "- **结果**: ❌ FAIL"
        else
            echo "- **结果**: ✅ PASS"
        fi
        echo "- **响应体**: \`\`\`json"
        echo "${resp_body}"
        echo "\`\`\`"
        echo ""
    } >> "${REPORT_FILE}"
}

echo "============================================"
echo "  墨竹 (Chrono-shift) 安全渗透测试"
echo "  目标: ${BASE_URL}"
echo "  时间: $(date '+%Y-%m-%d %H:%M:%S')"
echo "============================================"
echo ""

# ---- 初始化报告 ----
cat > "${REPORT_FILE}" << EOF
# 墨竹 安全渗透测试报告

**测试时间**: $(date '+%Y-%m-%d %H:%M:%S')
**测试目标**: ${BASE_URL}

---

## 测试结果汇总

| 类别 | 测试数 | 通过 | 失败 |
|------|--------|------|------|
EOF

# ============================================================
# 测试类别 1: SQL 注入测试
# ============================================================
echo ""
info "========== 类别 1: SQL 注入测试 =========="
echo "### 1. SQL 注入测试" >> "${REPORT_FILE}"

# 1.1 登录 SQL 注入 - 布尔型
run_test \
    "SQL注入 - 登录用户名 OR 1=1" \
    "POST" "/api/user/login" \
    '{"username":"admin'"'"' OR 1=1--","password":"test"}' \
    400 401 \
    "token"

# 1.2 登录 SQL 注入 - UNION
run_test \
    "SQL注入 - UNION SELECT" \
    "POST" "/api/user/login" \
    '{"username":"'"'"' UNION SELECT * FROM users--","password":"test"}' \
    400 401 \
    "token"

# 1.3 用户搜索 SQL 注入
run_test \
    "SQL注入 - 搜索参数" \
    "GET" "/api/user/search?q=test%27%20OR%201=1--" \
    "" 200 400 \
    "error"

# 1.4 注册 SQL 注入
run_test \
    "SQL注入 - 注册用户名" \
    "POST" "/api/user/register" \
    '{"username":"test'"'"'; DROP TABLE users--","password":"pass123","nickname":"hacker"}' \
    400 400 \
    "ok"

# ============================================================
# 测试类别 2: XSS 测试
# ============================================================
echo ""
info "========== 类别 2: XSS 测试 =========="
echo "### 2. XSS 测试" >> "${REPORT_FILE}"

# 2.1 注册 XSS - 用户名字段
run_test \
    "XSS - 用户名 script 标签" \
    "POST" "/api/user/register" \
    '{"username":"<script>alert(1)</script>","password":"pass123","nickname":"xss_test"}' \
    400 400 \
    "ok"

# 2.2 注册 XSS - 昵称字段
run_test \
    "XSS - 昵称 img onerror" \
    "POST" "/api/user/register" \
    '{"username":"xss_test2","password":"pass123","nickname":"<img src=x onerror=alert(1)>"}' \
    400 400 \
    "ok"

# 2.3 消息内容 XSS
run_test \
    "XSS - 消息内容" \
    "POST" "/api/message/send" \
    '{"to_user_id":1,"content":"<script>document.cookie</script>"}' \
    401 403 \
    "ok"

# 2.4 搜索 XSS
run_test \
    "XSS - 搜索参数" \
    "GET" "/api/user/search?q=%3Cscript%3Ealert(1)%3C/script%3E" \
    "" 200 200 \
    "<script>"

# ============================================================
# 测试类别 3: 路径遍历测试
# ============================================================
echo ""
info "========== 类别 3: 路径遍历测试 =========="
echo "### 3. 路径遍历测试" >> "${REPORT_FILE}"

# 3.1 路径遍历 - 模板下载
run_test \
    "路径遍历 - 模板下载 ../etc/passwd" \
    "GET" "/api/templates/download?id=../../../etc/passwd" \
    "" 400 404 \
    "root:"

# 3.2 路径遍历 - 用户资料
run_test \
    "路径遍历 - 用户头像路径" \
    "POST" "/api/user/update" \
    '{"avatar_url":"../../../windows/system32/drivers/etc/hosts"}' \
    401 403 \
    "127.0.0.1"

# 3.3 路径遍历 - Windows 系统文件
run_test \
    "路径遍历 - Windows hosts 文件" \
    "GET" "/api/templates/download?id=..\\..\\..\\windows\\system32\\drivers\\etc\\hosts" \
    "" 400 404 \
    "localhost"

# ============================================================
# 测试类别 4: JWT 伪造测试
# ============================================================
echo ""
info "========== 类别 4: JWT 伪造测试 =========="
echo "### 4. JWT 伪造测试" >> "${REPORT_FILE}"

# 4.1 空 Token
run_test \
    "JWT - 空 Authorization 头" \
    "GET" "/api/user/profile" \
    "" 401 401 \
    ""

# 4.2 无效 Token
run_test \
    "JWT - 随机字符串" \
    "GET" "/api/user/profile" \
    "" 401 401 \
    ""
# 需要额外添加 header，用 curl 直接测试
TOTAL=$((TOTAL+1))
info "测试 #${TOTAL}: JWT - 随机字符串 Token"
HTTP_CODE=$(curl -s -o "${REPORT_DIR}/.resp_body" -w "%{http_code}" \
    -H "Authorization: Bearer invalid_token_here" \
    "${BASE_URL}/api/user/profile")
if [ "${HTTP_CODE}" -ge 401 ] && [ "${HTTP_CODE}" -le 403 ]; then
    pass "JWT - 随机字符串 Token (HTTP ${HTTP_CODE})"
    RESULT="✅ PASS"
else
    fail "JWT - 随机字符串 Token (HTTP ${HTTP_CODE})"
    RESULT="❌ FAIL"
fi
{
    echo "### #${TOTAL}: JWT - 随机字符串 Token"
    echo "- **方法**: GET"
    echo "- **路径**: /api/user/profile"
    echo "- **HTTP 状态码**: ${HTTP_CODE}"
    echo "- **期望范围**: 401-403"
    echo "- **结果**: ${RESULT}"
    echo ""
} >> "${REPORT_FILE}"

# 4.3 算法混淆 Token (alg: none)
TOTAL=$((TOTAL+1))
info "测试 #${TOTAL}: JWT - alg:none 攻击"
# 构造 {"alg":"none","typ":"JWT"}.{"sub":"1","role":"admin"}.
local HEADER_B64=$(echo -n '{"alg":"none","typ":"JWT"}' | base64 -w0 | tr '+/' '-_' | tr -d '=')
local PAYLOAD_B64=$(echo -n '{"sub":"1","role":"admin","exp":9999999999}' | base64 -w0 | tr '+/' '-_' | tr -d '=')
local NONE_TOKEN="${HEADER_B64}.${PAYLOAD_B64}."
HTTP_CODE=$(curl -s -o "${REPORT_DIR}/.resp_body" -w "%{http_code}" \
    -H "Authorization: Bearer ${NONE_TOKEN}" \
    "${BASE_URL}/api/user/profile")
if [ "${HTTP_CODE}" -ge 401 ] && [ "${HTTP_CODE}" -le 403 ]; then
    pass "JWT - alg:none 攻击 (HTTP ${HTTP_CODE})"
    RESULT="✅ PASS"
else
    fail "JWT - alg:none 攻击 (HTTP ${HTTP_CODE})"
    RESULT="❌ FAIL"
fi
{
    echo "### #${TOTAL}: JWT - alg:none 攻击"
    echo "- **方法**: GET"
    echo "- **路径**: /api/user/profile"
    echo "- **Token**: \`${NONE_TOKEN:0:50}...\`"
    echo "- **HTTP 状态码**: ${HTTP_CODE}"
    echo "- **期望范围**: 401-403"
    echo "- **结果**: ${RESULT}"
    echo ""
} >> "${REPORT_FILE}"

# 4.4 过期 Token
TOTAL=$((TOTAL+1))
info "测试 #${TOTAL}: JWT - 过期 Token"
# 构造过期 JWT (使用固定密钥测试, exp=1000000000 即 2001年)
local EXP_HEADER=$(echo -n '{"alg":"HS256","typ":"JWT"}' | base64 -w0 | tr '+/' '-_' | tr -d '=')
local EXP_PAYLOAD=$(echo -n '{"sub":"1","exp":1000000000}' | base64 -w0 | tr '+/' '-_' | tr -d '=')
local EXP_TOKEN="${EXP_HEADER}.${EXP_PAYLOAD}.fakesignature"
HTTP_CODE=$(curl -s -o "${REPORT_DIR}/.resp_body" -w "%{http_code}" \
    -H "Authorization: Bearer ${EXP_TOKEN}" \
    "${BASE_URL}/api/user/profile")
if [ "${HTTP_CODE}" -ge 401 ] && [ "${HTTP_CODE}" -le 403 ]; then
    pass "JWT - 过期 Token (HTTP ${HTTP_CODE})"
    RESULT="✅ PASS"
else
    fail "JWT - 过期 Token (HTTP ${HTTP_CODE})"
    RESULT="❌ FAIL"
fi
{
    echo "### #${TOTAL}: JWT - 过期 Token"
    echo "- **方法**: GET"
    echo "- **路径**: /api/user/profile"
    echo "- **HTTP 状态码**: ${HTTP_CODE}"
    echo "- **期望范围**: 401-403"
    echo "- **结果**: ${RESULT}"
    echo ""
} >> "${REPORT_FILE}"

# ============================================================
# 测试类别 5: 权限越界测试
# ============================================================
echo ""
info "========== 类别 5: 权限越界测试 =========="
echo "### 5. 权限越界测试" >> "${REPORT_FILE}"

# 5.1 未认证访问需认证端点
run_test \
    "权限越界 - 未认证访问消息发送" \
    "POST" "/api/message/send" \
    '{"to_user_id":2,"content":"test"}' \
    401 401 \
    "ok"

# 5.2 未认证访问好友列表
run_test \
    "权限越界 - 未认证访问好友列表" \
    "GET" "/api/user/friends" \
    "" 401 401 \
    ""

# 5.3 未认证访问模板应用
run_test \
    "权限越界 - 未认证应用模板" \
    "POST" "/api/templates/apply" \
    '{"template_id":1}' \
    401 401 \
    ""

# ============================================================
# 测试类别 6: 大 Payload 测试
# ============================================================
echo ""
info "========== 类别 6: 大 Payload 测试 =========="
echo "### 6. 大 Payload 测试" >> "${REPORT_FILE}"

# 6.1 超大用户名
local BIG_USERNAME=$(python3 -c "print('A'*10000)" 2>/dev/null || printf 'A%.0s' {1..1000})
run_test \
    "大 Payload - 超长用户名" \
    "POST" "/api/user/register" \
    "{\"username\":\"${BIG_USERNAME:0:1000}\",\"password\":\"pass123\",\"nickname\":\"big\"}" \
    400 413 \
    "ok"

# 6.2 超长消息内容
local BIG_MSG=$(python3 -c "print('M'*100000)" 2>/dev/null || printf 'M%.0s' {1..10000})
run_test \
    "大 Payload - 超长消息" \
    "POST" "/api/message/send" \
    "{\"to_user_id\":1,\"content\":\"${BIG_MSG:0:10000}\"}" \
    401 413 \
    "ok"

# ============================================================
# 测试类别 7: 输入验证测试
# ============================================================
echo ""
info "========== 类别 7: 输入验证测试 =========="
echo "### 7. 输入验证测试" >> "${REPORT_FILE}"

# 7.1 空用户名
run_test \
    "输入验证 - 空用户名" \
    "POST" "/api/user/register" \
    '{"username":"","password":"pass123","nickname":"empty"}' \
    400 400 \
    "ok"

# 7.2 空密码
run_test \
    "输入验证 - 空密码" \
    "POST" "/api/user/login" \
    '{"username":"admin","password":""}' \
    400 400 \
    "ok"

# 7.3 无效 JSON
run_test \
    "输入验证 - 无效 JSON" \
    "POST" "/api/user/register" \
    'not json at all' \
    400 400 \
    "error"

# 7.4 特殊字符
run_test \
    "输入验证 - 特殊字符" \
    "POST" "/api/user/register" \
    '{"username":"test@#$%^&*()","password":"pass123","nickname":"special"}' \
    400 400 \
    "ok"

# ============================================================
# 生成汇总报告
# ============================================================
echo ""
echo "============================================"
echo "  测试完成"
echo "  总测试数: ${TOTAL}"
echo -e "  通过: ${GREEN}${PASS}${NC}"
echo -e "  失败: ${RED}${FAIL}${NC}"
echo "============================================"

# 更新报告汇总表
cat >> "${REPORT_FILE}" << EOF

---

## 测试结果统计

| 指标 | 值 |
|------|-----|
| 总测试数 | ${TOTAL} |
| ✅ 通过 | ${PASS} |
| ❌ 失败 | ${FAIL} |
| 通过率 | $(printf "%.1f" $(echo "scale=2; ${PASS} * 100 / ${TOTAL}" | bc 2>/dev/null || echo "0"))% |
| 测试时间 | $(date '+%Y-%m-%d %H:%M:%S') |

EOF

if [ -f "${REPORT_DIR}/.failures" ]; then
    echo -e "\n${YELLOW}失败详情:${NC}"
    cat "${REPORT_DIR}/.failures"
    cat >> "${REPORT_FILE}" << EOF

## 失败测试详情

\`\`\`
$(cat "${REPORT_DIR}/.failures")
\`\`\`
EOF
fi

# 清理临时文件
rm -f "${REPORT_DIR}/.resp_body" "${REPORT_DIR}/.failures"

echo ""
echo "报告已保存: ${REPORT_FILE}"

# 退出码
if [ "${FAIL}" -gt 0 ]; then
    exit 1
fi
exit 0
