#!/bin/bash
# ============================================================
# 墨竹 (Chrono-shift) 客户端预留接口验证脚本
# 验证所有 IPC 消息类型和 HTTP API 端点的可用性
# ============================================================

REPORT_DIR="reports"
REPORT_FILE="${REPORT_DIR}/api_verification_results.md"
HOST="127.0.0.1"
PORT="4443"
BASE_URL="https://${HOST}:${PORT}"
PASS=0
FAIL=0
TOTAL=0
TEST_TOKEN=""

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

# ---- 测试辅助函数 ----
run_api_test() {
    local test_name="$1"
    local method="$2"
    local path="$3"
    local body="$4"
    local token="$5"
    local expected_code="$6"

    TOTAL=$((TOTAL+1))
    info "测试 #${TOTAL}: [${method}] ${path}"

    local cmd="curl -s -o \"${REPORT_DIR}/.resp_body\" -w \"%{http_code}\" -X ${method} \"${BASE_URL}${path}\""
    if [ -n "${body}" ]; then
        cmd="${cmd} -H \"Content-Type: application/json\" -d '${body}'"
    fi
    if [ -n "${token}" ]; then
        cmd="${cmd} -H \"Authorization: Bearer ${token}\""
    fi

    local http_code=$(eval ${cmd})
    local resp_body=$(cat "${REPORT_DIR}/.resp_body" 2>/dev/null)

    if [ "${http_code}" -eq "${expected_code}" ]; then
        pass "${test_name} (HTTP ${http_code})"
        RESULT="✅ PASS"
    else
        fail "${test_name} (期望 ${expected_code}, 实际 ${http_code})"
        RESULT="❌ FAIL"
    fi

    {
        echo "### #${TOTAL}: ${test_name}"
        echo "- **方法**: ${method}"
        echo "- **路径**: ${path}"
        if [ -n "${body}" ]; then echo "- **请求体**: \`${body}\`"; fi
        echo "- **期望状态码**: ${expected_code}"
        echo "- **实际状态码**: ${http_code}"
        echo "- **结果**: ${RESULT}"
        echo "- **响应体**:"
        echo "\`\`\`json"
        echo "${resp_body}"
        echo "\`\`\`"
        echo ""
    } >> "${REPORT_FILE}"
}

# ---- 初始化报告 ----
cat > "${REPORT_FILE}" << EOF
# 墨竹 客户端预留接口验证报告

**测试时间**: $(date '+%Y-%m-%d %H:%M:%S')
**测试目标**: ${BASE_URL}

---

## 1. IPC 消息类型定义

| 类型码 | 名称 | 描述 | 状态 |
|--------|------|------|------|
| 0x01 | IPC_LOGIN | 用户登录 | 已定义 |
| 0x02 | IPC_LOGOUT | 用户登出 | 已定义 |
| 0x10 | IPC_SEND_MESSAGE | 发送消息 | 已定义 |
| 0x11 | IPC_GET_MESSAGES | 获取消息历史 | 已定义 |
| 0x20 | IPC_GET_CONTACTS | 获取联系人列表 | 已定义 |
| 0x30 | IPC_GET_TEMPLATES | 获取社区模板 | 已定义 |
| 0x31 | IPC_APPLY_TEMPLATE | 应用模板主题 | 已定义 |
| 0x40 | IPC_FILE_UPLOAD | 上传文件 | 已定义 |
| 0xFF | IPC_SYSTEM_NOTIFY | 系统通知 | 已定义 |

> IPC 消息类型在 \`client/include/ipc_bridge.h\` 和 \`client/ui/js/ipc.js\` 中定义，
> 通过 \`ipc_bridge.c\` 实现了基础的注册/分发框架，\`ipc_send_to_js\` 暂为 stub。

## 2. HTTP API 端点验证

EOF

echo "============================================"
echo "  墨竹 API 验证测试"
echo "  目标: ${BASE_URL}"
echo "============================================"
echo ""

# ============================================================
# 2.1 健康检查
# ============================================================
info "========== 2.1 健康检查 =========="
echo "### 2.1 健康检查" >> "${REPORT_FILE}"

run_api_test "健康检查" "GET" "/api/health" "" "" 200

# ============================================================
# 2.2 用户系统
# ============================================================
info "========== 2.2 用户系统 =========="
echo "### 2.2 用户系统" >> "${REPORT_FILE}"

# 先清理测试用户
run_api_test "用户注册" "POST" "/api/user/register" \
    '{"username":"api_test_user","password":"Test123456","nickname":"API测试"}' \
    "" 200

run_api_test "重复注册 - 应返回冲突" "POST" "/api/user/register" \
    '{"username":"api_test_user","password":"Test123456","nickname":"API测试"}' \
    "" 409

# 登录获取 token
TOTAL=$((TOTAL+1))
info "测试 #${TOTAL}: 用户登录"
HTTP_CODE=$(curl -s -o "${REPORT_DIR}/.resp_body" -w "%{http_code}" \
    -X POST -H "Content-Type: application/json" \
    -d '{"username":"api_test_user","password":"Test123456"}' \
    "${BASE_URL}/api/user/login")
RESP_BODY=$(cat "${REPORT_DIR}/.resp_body" 2>/dev/null)
TEST_TOKEN=$(echo "${RESP_BODY}" | python3 -c "import sys,json; print(json.load(sys.stdin).get('data',{}).get('token',''))" 2>/dev/null)

if [ "${HTTP_CODE}" -eq 200 ] && [ -n "${TEST_TOKEN}" ]; then
    pass "用户登录 (HTTP ${HTTP_CODE}, Token 已获取)"
    RESULT="✅ PASS"
else
    fail "用户登录 (HTTP ${HTTP_CODE})"
    RESULT="❌ FAIL"
fi
{
    echo "### #${TOTAL}: 用户登录"
    echo "- **方法**: POST"
    echo "- **路径**: /api/user/login"
    echo "- **请求体**: \`{\"username\":\"api_test_user\",\"password\":\"Test123456\"}\`"
    echo "- **期望状态码**: 200"
    echo "- **实际状态码**: ${HTTP_CODE}"
    echo "- **结果**: ${RESULT}"
    echo "- **响应体**:"
    echo "\`\`\`json"
    echo "${RESP_BODY}"
    echo "\`\`\`"
    echo ""
} >> "${REPORT_FILE}"

# 带 Token 的测试
run_api_test "获取用户信息" "GET" "/api/user/profile" "" "${TEST_TOKEN}" 200

run_api_test "更新用户信息" "PUT" "/api/user/update" \
    '{"nickname":"API测试已更新"}' "${TEST_TOKEN}" 200

run_api_test "搜索用户" "GET" "/api/user/search?q=API" "" "${TEST_TOKEN}" 200

run_api_test "获取好友列表" "GET" "/api/user/friends" "" "${TEST_TOKEN}" 200

# ============================================================
# 2.3 消息系统
# ============================================================
info "========== 2.3 消息系统 =========="
echo "### 2.3 消息系统" >> "${REPORT_FILE}"

run_api_test "发送消息" "POST" "/api/message/send" \
    '{"to_user_id":1,"content":"测试消息内容"}' "${TEST_TOKEN}" 200

run_api_test "获取消息历史" "GET" "/api/message/list?user_id=1&offset=0&limit=50" "" "${TEST_TOKEN}" 200

# ============================================================
# 2.4 模板系统
# ============================================================
info "========== 2.4 模板系统 =========="
echo "### 2.4 模板系统" >> "${REPORT_FILE}"

run_api_test "获取模板列表" "GET" "/api/templates?offset=0&limit=20" "" "" 200

run_api_test "应用模板" "POST" "/api/templates/apply" \
    '{"template_id":1}' "${TEST_TOKEN}" 200

# ============================================================
# 2.5 边界情况
# ============================================================
info "========== 2.5 边界情况 =========="
echo "### 2.5 边界情况" >> "${REPORT_FILE}"

run_api_test "未认证访问 - 好友列表" "GET" "/api/user/friends" "" "" 401

run_api_test "未认证访问 - 消息发送" "POST" "/api/message/send" \
    '{"to_user_id":1,"content":"test"}' "" 401

run_api_test "无效路径" "GET" "/api/invalid/path" "" "" 404

run_api_test "不支持的 HTTP 方法" "DELETE" "/api/health" "" "" 405

# ============================================================
# 2.6 文件上传/下载
# ============================================================
info "========== 2.6 文件系统 =========="
echo "### 2.6 文件系统" >> "${REPORT_FILE}"

run_api_test "上传文件 - 文本文件" "POST" "/api/file/upload" \
    '{"filename":"test.txt","content":"dGVzdCBmaWxlIGNvbnRlbnQ="}' "${TEST_TOKEN}" 200

# 先获取一个文件ID用于下载测试
TOTAL=$((TOTAL+1))
info "测试 #${TOTAL}: 获取文件列表"
HTTP_CODE=$(curl -s -o "${REPORT_DIR}/.resp_body" -w "%{http_code}" \
    -H "Authorization: Bearer ${TEST_TOKEN}" \
    "${BASE_URL}/api/files")
RESP_BODY=$(cat "${REPORT_DIR}/.resp_body" 2>/dev/null)
FILE_ID=$(echo "${RESP_BODY}" | python3 -c "import sys,json; d=json.load(sys.stdin); print(d.get('data',[{}])[0].get('id','') if isinstance(d.get('data'),list) else '')" 2>/dev/null || echo "")
if [ "${HTTP_CODE}" -eq 200 ]; then
    pass "获取文件列表 (HTTP ${HTTP_CODE})"
    RESULT="✅ PASS"
    if [ -n "${FILE_ID}" ]; then
        run_api_test "下载文件" "GET" "/api/file/download?id=${FILE_ID}" "" "${TEST_TOKEN}" 200
    fi
else
    fail "获取文件列表 (HTTP ${HTTP_CODE})"
    RESULT="❌ FAIL"
fi
{
    echo "### #${TOTAL}: 获取文件列表"
    echo "- **方法**: GET"
    echo "- **路径**: /api/files"
    echo "- **HTTP 状态码**: ${HTTP_CODE}"
    echo "- **结果**: ${RESULT}"
    echo ""
} >> "${REPORT_FILE}"

run_api_test "上传头像" "POST" "/api/user/avatar/upload" \
    '{"avatar_data":"iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNk+M9QDwADhgGAWjR9awAAAABJRU5ErkJggg=="}' "${TEST_TOKEN}" 200

run_api_test "未认证 - 文件上传" "POST" "/api/file/upload" \
    '{"filename":"hack.txt","content":"ZXZpbA=="}' "" 401

# ============================================================
# 2.7 好友系统
# ============================================================
info "========== 2.7 好友系统 =========="
echo "### 2.7 好友系统" >> "${REPORT_FILE}"

# 注册第二个用户用于好友测试
TOTAL=$((TOTAL+1))
info "测试 #${TOTAL}: 注册好友测试用户"
HTTP_CODE=$(curl -s -o "${REPORT_DIR}/.resp_body" -w "%{http_code}" \
    -X POST -H "Content-Type: application/json" \
    -d '{"username":"friend_test_user","password":"Friend123456","nickname":"好友测试"}' \
    "${BASE_URL}/api/user/register")
RESP_BODY=$(cat "${REPORT_DIR}/.resp_body" 2>/dev/null)
FRIEND_USER_ID=$(echo "${RESP_BODY}" | python3 -c "import sys,json; print(json.load(sys.stdin).get('data',{}).get('user_id',''))" 2>/dev/null || echo "")
if [ "${HTTP_CODE}" -eq 200 ] || [ "${HTTP_CODE}" -eq 409 ]; then
    pass "好友测试用户注册 (HTTP ${HTTP_CODE})"
    RESULT="✅ PASS"
else
    fail "好友测试用户注册 (HTTP ${HTTP_CODE})"
    RESULT="❌ FAIL"
fi
{
    echo "### #${TOTAL}: 注册好友测试用户"
    echo "- **方法**: POST"
    echo "- **路径**: /api/user/register"
    echo "- **HTTP 状态码**: ${HTTP_CODE}"
    echo "- **结果**: ${RESULT}"
    echo ""
} >> "${REPORT_FILE}"

# 如果不知道好友ID，用已存在的用户ID进行测试
FRIEND_TEST_ID="${FRIEND_USER_ID:-1}"
run_api_test "添加好友" "POST" "/api/friends/add" \
    "{\"user_id1\":1,\"user_id2\":${FRIEND_TEST_ID}}" "${TEST_TOKEN}" 200

run_api_test "获取好友列表" "GET" "/api/friends?id=1" "" "${TEST_TOKEN}" 200

# ============================================================
# 2.8 社区模板 CRUD
# ============================================================
info "========== 2.8 社区模板 =========="
echo "### 2.8 社区模板 CRUD" >> "${REPORT_FILE}"

run_api_test "创建模板" "POST" "/api/templates" \
    '{"name":"API测试模板","description":"通过API验证创建的模板","content":"{\"primary\":\"#12B7F5\"}"}' "${TEST_TOKEN}" 200

# 获取模板ID
TOTAL=$((TOTAL+1))
info "测试 #${TOTAL}: 获取模板列表 (含新模板)"
HTTP_CODE=$(curl -s -o "${REPORT_DIR}/.resp_body" -w "%{http_code}" \
    -H "Authorization: Bearer ${TEST_TOKEN}" \
    "${BASE_URL}/api/templates?limit=100&offset=0")
RESP_BODY=$(cat "${REPORT_DIR}/.resp_body" 2>/dev/null)
TPL_ID=$(echo "${RESP_BODY}" | python3 -c "import sys,json; d=json.load(sys.stdin); items=d.get('data',{}).get('templates',[]) if isinstance(d.get('data'),dict) else d.get('data',[]); print(items[-1].get('id','') if items else '')" 2>/dev/null || echo "")
if [ "${HTTP_CODE}" -eq 200 ]; then
    pass "获取模板列表 (HTTP ${HTTP_CODE})"
    RESULT="✅ PASS"
else
    fail "获取模板列表 (HTTP ${HTTP_CODE})"
    RESULT="❌ FAIL"
fi
{
    echo "### #${TOTAL}: 获取模板列表"
    echo "- **方法**: GET"
    echo "- **路径**: /api/templates"
    echo "- **HTTP 状态码**: ${HTTP_CODE}"
    echo "- **结果**: ${RESULT}"
    echo ""
} >> "${REPORT_FILE}"

if [ -n "${TPL_ID}" ]; then
    run_api_test "更新模板" "PUT" "/api/templates/${TPL_ID}" \
        '{"name":"API测试模板已更新","description":"更新描述"}' "${TEST_TOKEN}" 200
    
    run_api_test "应用模板" "POST" "/api/templates/apply" \
        "{\"template_id\":${TPL_ID}}" "${TEST_TOKEN}" 200
    
    run_api_test "删除模板" "DELETE" "/api/templates/${TPL_ID}" "" "${TEST_TOKEN}" 200
fi

# ============================================================
# 3. 清理测试用户
# ============================================================
info "========== 清理测试数据 =========="
run_api_test "删除测试用户" "DELETE" "/api/user?id=0" "" "${TEST_TOKEN}" 200

# ============================================================
# 生成汇总
# ============================================================
echo ""
echo "============================================"
echo "  API 验证完成"
echo "  总测试数: ${TOTAL}"
echo -e "  通过: ${GREEN}${PASS}${NC}"
echo -e "  失败: ${RED}${FAIL}${NC}"
echo "============================================"

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

rm -f "${REPORT_DIR}/.resp_body"

echo ""
echo "报告已保存: ${REPORT_FILE}"
exit $([ "${FAIL}" -gt 0 ] && echo 1 || echo 0)
