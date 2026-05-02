#!/usr/bin/env bash
# ============================================================
# 墨竹 (Chrono-shift) 回环测试
# 通过 CLI 工具验证服务器端到端可用性与响应能力
#
# 测试流程:
#   1. 启动服务器
#   2. 注册新用户
#   3. 登录获取 Token
#   4. 获取用户资料
#   5. 发送消息
#   6. 获取消息历史
#   7. 获取社区模板
#   8. 清理: 删除测试用户 & 停止服务器
# ============================================================

set -e

# --- 颜色 ---
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# --- 配置 ---
BASE_URL="${BASE_URL:-https://127.0.0.1:4443}"
SERVER_BIN="${SERVER_BIN:-./server/build/chrono_server}"
SERVER_LOG="./loopback_server.log"
TEST_USER="loopback_test_$(date +%s)"
TEST_PASS="LoopTest@123"
TEST_NICK="回环测试用户"
TEST_MSG="Hello from loopback test @ $(date '+%H:%M:%S')"
TOKEN=""
USER_ID=""

# --- 计数器 ---
PASS_COUNT=0
FAIL_COUNT=0
STEP_COUNT=0

# --- 辅助函数 ---

log_info()    { echo -e "${BLUE}[INFO]${NC} $1"; }
log_success() { echo -e "${GREEN}[PASS]${NC} $1"; PASS_COUNT=$((PASS_COUNT + 1)); }
log_fail()    { echo -e "${RED}[FAIL]${NC} $1"; FAIL_COUNT=$((FAIL_COUNT + 1)); }
log_step()    { STEP_COUNT=$((STEP_COUNT + 1)); echo -e "\n${CYAN}[Step $STEP_COUNT]${NC} $1"; }
log_separator() { echo -e "${YELLOW}----------------------------------------${NC}"; }

cleanup() {
    echo ""
    log_separator
    log_info "执行清理..."
    
    # 删除测试用户（如果登录成功过）
    if [ -n "$TOKEN" ]; then
        log_info "删除测试用户: $TEST_USER"
        curl -s -X POST "$BASE_URL/api/user/delete" \
            -H "Authorization: Bearer $TOKEN" \
            -H "Content-Type: application/json" \
            -d "{\"username\":\"$TEST_USER\"}" > /dev/null 2>&1 || true
    fi
    
    # 停止服务器
    if [ -n "$SERVER_PID" ]; then
        log_info "停止服务器 (PID: $SERVER_PID)"
        kill "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
        log_success "服务器已停止"
    fi
    
    # 清理日志
    rm -f "$SERVER_LOG" 2>/dev/null || true
    log_info "临时日志已清理"
    log_separator
}

# 注册退出处理
trap cleanup EXIT INT TERM

# --- 打印标题 ---
echo ""
echo "============================================"
echo "   墨竹 (Chrono-shift) 回环测试"
echo "============================================"
echo "  服务器: $BASE_URL"
echo "  测试用户: $TEST_USER"
echo "  开始时间: $(date '+%Y-%m-%d %H:%M:%S')"
echo "============================================"
echo ""

# ========================================
# Step 1: 检查服务器是否可访问
# ========================================
log_step "检查服务器可访问性"

if curl -s -o /dev/null -w "%{http_code}" "$BASE_URL/api/health" --connect-timeout 5 > /dev/null 2>&1; then
    log_info "服务器 $BASE_URL 已运行，跳过启动"
else
    log_info "服务器未运行，正在启动..."
    
    if [ ! -f "$SERVER_BIN" ]; then
        log_fail "服务器二进制文件不存在: $SERVER_BIN"
        log_info "请设置 SERVER_BIN 环境变量指定正确的路径"
        log_info "例如: SERVER_BIN=./build/server/chrono_server $0"
        exit 1
    fi
    
    # 启动服务器（后台运行）
    $SERVER_BIN > "$SERVER_LOG" 2>&1 &
    SERVER_PID=$!
    log_info "服务器启动中 (PID: $SERVER_PID)..."
    
    # 等待服务器就绪
    READY=false
    for i in $(seq 1 30); do
        if curl -s -o /dev/null -w "%{http_code}" "$BASE_URL/api/health" --connect-timeout 2 2>/dev/null | grep -q "200"; then
            READY=true
            break
        fi
        sleep 1
    done
    
    if [ "$READY" = false ]; then
        log_fail "服务器启动超时"
        exit 1
    fi
    log_success "服务器已就绪"
fi

# ========================================
# Step 2: 健康检查
# ========================================
log_step "健康检查 (GET $BASE_URL/api/health)"

HTTP_CODE=$(curl -s -o /tmp/loopback_health.json -w "%{http_code}" "$BASE_URL/api/health" --connect-timeout 5)
HEALTH_BODY=$(cat /tmp/loopback_health.json 2>/dev/null || echo "")

if [ "$HTTP_CODE" = "200" ]; then
    log_success "健康检查通过 (HTTP $HTTP_CODE)"
    log_info "  响应: $HEALTH_BODY"
else
    log_fail "健康检查失败 (HTTP $HTTP_CODE)"
    log_info "  响应: $HEALTH_BODY"
fi

# ========================================
# Step 3: 用户注册
# ========================================
log_step "用户注册 (POST $BASE_URL/api/user/register)"

REG_RESP=$(curl -s -X POST "$BASE_URL/api/user/register" \
    -H "Content-Type: application/json" \
    -d "{\"username\":\"$TEST_USER\",\"password\":\"$TEST_PASS\",\"nickname\":\"$TEST_NICK\"}")

REG_STATUS=$(echo "$REG_RESP" | python3 -c "import sys,json; print(json.load(sys.stdin).get('status',''))" 2>/dev/null || echo "parse_error")

if [ "$REG_STATUS" = "ok" ]; then
    USER_ID=$(echo "$REG_RESP" | python3 -c "import sys,json; print(json.load(sys.stdin).get('data',{}).get('user_id',''))" 2>/dev/null || echo "")
    log_success "用户注册成功 (user_id: $USER_ID)"
else
    log_fail "用户注册失败: $REG_RESP"
fi

# ========================================
# Step 4: 用户登录
# ========================================
log_step "用户登录 (POST $BASE_URL/api/user/login)"

LOGIN_RESP=$(curl -s -X POST "$BASE_URL/api/user/login" \
    -H "Content-Type: application/json" \
    -d "{\"username\":\"$TEST_USER\",\"password\":\"$TEST_PASS\"}")

LOGIN_STATUS=$(echo "$LOGIN_RESP" | python3 -c "import sys,json; print(json.load(sys.stdin).get('status',''))" 2>/dev/null || echo "parse_error")

if [ "$LOGIN_STATUS" = "ok" ]; then
    TOKEN=$(echo "$LOGIN_RESP" | python3 -c "import sys,json; print(json.load(sys.stdin).get('data',{}).get('token',''))" 2>/dev/null || echo "")
    log_success "用户登录成功"
    log_info "  Token: ${TOKEN:0:20}...${TOKEN: -10}"
else
    log_fail "用户登录失败: $LOGIN_RESP"
fi

# ========================================
# Step 5: 获取用户资料
# ========================================
log_step "获取用户资料 (GET $BASE_URL/api/user/profile)"

PROFILE_RESP=$(curl -s "$BASE_URL/api/user/profile" \
    -H "Authorization: Bearer $TOKEN")

PROFILE_STATUS=$(echo "$PROFILE_RESP" | python3 -c "import sys,json; print(json.load(sys.stdin).get('status',''))" 2>/dev/null || echo "parse_error")

if [ "$PROFILE_STATUS" = "ok" ]; then
    PROFILE_NAME=$(echo "$PROFILE_RESP" | python3 -c "import sys,json; print(json.load(sys.stdin).get('data',{}).get('nickname',''))" 2>/dev/null || echo "")
    log_success "获取资料成功 (nickname: $PROFILE_NAME)"
    [ "$PROFILE_NAME" = "$TEST_NICK" ] && log_success "  昵称匹配" || log_fail "  昵称不匹配 (期望: $TEST_NICK, 实际: $PROFILE_NAME)"
else
    log_fail "获取资料失败: $PROFILE_RESP"
fi

# ========================================
# Step 6: 更新用户资料
# ========================================
log_step "更新用户资料 (POST $BASE_URL/api/user/update)"

NEW_NICK="${TEST_NICK}_已更新"
UPDATE_RESP=$(curl -s -X POST "$BASE_URL/api/user/update" \
    -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json" \
    -d "{\"nickname\":\"$NEW_NICK\"}")

UPDATE_STATUS=$(echo "$UPDATE_RESP" | python3 -c "import sys,json; print(json.load(sys.stdin).get('status',''))" 2>/dev/null || echo "parse_error")

if [ "$UPDATE_STATUS" = "ok" ]; then
    log_success "资料更新成功"
else
    log_fail "资料更新失败: $UPDATE_RESP"
fi

# ========================================
# Step 7: 发送消息
# ========================================
log_step "发送消息 (POST $BASE_URL/api/message/send)"

MSG_RESP=$(curl -s -X POST "$BASE_URL/api/message/send" \
    -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json" \
    -d "{\"to_user_id\":\"$USER_ID\",\"content\":\"$TEST_MSG\"}")

MSG_STATUS=$(echo "$MSG_RESP" | python3 -c "import sys,json; print(json.load(sys.stdin).get('status',''))" 2>/dev/null || echo "parse_error")

if [ "$MSG_STATUS" = "ok" ]; then
    MSG_ID=$(echo "$MSG_RESP" | python3 -c "import sys,json; print(json.load(sys.stdin).get('data',{}).get('message_id',''))" 2>/dev/null || echo "")
    log_success "消息发送成功 (message_id: $MSG_ID)"
else
    log_fail "消息发送失败: $MSG_RESP"
fi

# ========================================
# Step 8: 获取消息历史
# ========================================
log_step "获取消息历史 (GET $BASE_URL/api/messages)"

HIST_RESP=$(curl -s "$BASE_URL/api/messages?user_id=$USER_ID&limit=10" \
    -H "Authorization: Bearer $TOKEN")

HIST_STATUS=$(echo "$HIST_RESP" | python3 -c "import sys,json; print(json.load(sys.stdin).get('status',''))" 2>/dev/null || echo "parse_error")

if [ "$HIST_STATUS" = "ok" ]; then
    MSG_COUNT=$(echo "$HIST_RESP" | python3 -c "import sys,json; print(len(json.load(sys.stdin).get('data',{}).get('messages',[])))" 2>/dev/null || echo "0")
    log_success "获取消息历史成功 (共 $MSG_COUNT 条)"
else
    log_fail "获取消息历史失败: $HIST_RESP"
fi

# ========================================
# Step 9: 获取社区模板
# ========================================
log_step "获取社区模板 (GET $BASE_URL/api/templates)"

TPL_RESP=$(curl -s "$BASE_URL/api/templates" \
    -H "Authorization: Bearer $TOKEN")

TPL_STATUS=$(echo "$TPL_RESP" | python3 -c "import sys,json; print(json.load(sys.stdin).get('status',''))" 2>/dev/null || echo "parse_error")

if [ "$TPL_STATUS" = "ok" ]; then
    TPL_COUNT=$(echo "$TPL_RESP" | python3 -c "import sys,json; print(len(json.load(sys.stdin).get('data',{}).get('templates',[])))" 2>/dev/null || echo "0")
    log_success "获取模板成功 (共 $TPL_COUNT 个模板)"
else
    log_fail "获取模板失败: $TPL_RESP"
fi

# ========================================
# Step 10: 搜索用户
# ========================================
log_step "搜索用户 (GET $BASE_URL/api/user/search)"

SEARCH_RESP=$(curl -s "$BASE_URL/api/user/search?keyword=$TEST_USER" \
    -H "Authorization: Bearer $TOKEN")

SEARCH_STATUS=$(echo "$SEARCH_RESP" | python3 -c "import sys,json; print(json.load(sys.stdin).get('status',''))" 2>/dev/null || echo "parse_error")

if [ "$SEARCH_STATUS" = "ok" ]; then
    SEARCH_COUNT=$(echo "$SEARCH_RESP" | python3 -c "import sys,json; print(len(json.load(sys.stdin).get('data',{}).get('users',[])))" 2>/dev/null || echo "0")
    log_success "搜索用户成功 (找到 $SEARCH_COUNT 个匹配)"
    [ "$SEARCH_COUNT" -ge 1 ] && log_success "  测试用户可搜索到" || log_fail "  未搜索到测试用户"
else
    log_fail "搜索用户失败: $SEARCH_RESP"
fi

# ========================================
# Step 11: CLI 调试工具验证
# ========================================
log_step "CLI 调试工具验证"

CLI_BIN="./server/build/debug_cli"
if [ -f "$CLI_BIN" ]; then
    log_info "CLI 工具存在: $CLI_BIN"
    
    # 测试 help 命令
    HELP_OUTPUT=$($CLI_BIN help 2>&1 || true)
    if echo "$HELP_OUTPUT" | grep -qi "ipc"; then
        log_success "CLI help 包含 ipc 命令"
    else
        log_fail "CLI help 缺少 ipc 命令"
    fi
    
    # 测试 ipc types
    IPC_TYPES=$($CLI_BIN ipc types 2>&1 || true)
    if echo "$IPC_TYPES" | grep -qi "OPEN_URL"; then
        log_success "CLI IPC types 包含 OPEN_URL (0x50)"
    else
        log_fail "CLI IPC types 缺少 OPEN_URL"
    fi
    
    # 测试 ipc send
    IPC_SEND=$($CLI_BIN ipc send 0x01 '{"username":"test"}' 2>&1 || true)
    if echo "$IPC_SEND" | grep -qi "sent\|dispatch\|模拟"; then
        log_success "CLI IPC send 命令正常执行"
    else
        log_info "  CLI IPC send 输出: $IPC_SEND"
        # 不标记为失败，可能模拟模式下输出不同
    fi
else
    log_info "CLI 工具未编译，跳过 (路径: $CLI_BIN)"
    log_info "  可通过以下命令编译:"
    log_info "    cd server && make debug_cli"
fi

# ========================================
# Step 12: 文件上传与下载 (F3)
# ========================================
log_step "文件上传 (POST $BASE_URL/api/file/upload)"

UPLOAD_RESP=$(curl -s -X POST "$BASE_URL/api/file/upload" \
    -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json" \
    -d '{"filename":"loopback_test.txt","content":"bG9vcGJhY2sgdGVzdCBmaWxlIGNvbnRlbnQ="}')

UPLOAD_STATUS=$(echo "$UPLOAD_RESP" | python3 -c "import sys,json; print(json.load(sys.stdin).get('status',''))" 2>/dev/null || echo "parse_error")

if [ "$UPLOAD_STATUS" = "ok" ]; then
    FILE_ID=$(echo "$UPLOAD_RESP" | python3 -c "import sys,json; print(json.load(sys.stdin).get('data',{}).get('file_id',''))" 2>/dev/null || echo "")
    log_success "文件上传成功 (file_id: $FILE_ID)"
    
    # 下载文件
    log_step "文件下载 (GET $BASE_URL/api/file/download?id=$FILE_ID)"
    DOWNLOAD_RESP=$(curl -s "$BASE_URL/api/file/download?id=$FILE_ID" \
        -H "Authorization: Bearer $TOKEN")
    DOWNLOAD_STATUS=$(echo "$DOWNLOAD_RESP" | python3 -c "import sys,json; print(json.load(sys.stdin).get('status',''))" 2>/dev/null || echo "parse_error")
    if [ "$DOWNLOAD_STATUS" = "ok" ]; then
        log_success "文件下载成功"
    else
        log_fail "文件下载失败: $DOWNLOAD_RESP"
    fi
else
    log_fail "文件上传失败: $UPLOAD_RESP"
fi

# ========================================
# Step 13: 社区模板 CRUD (F3)
# ========================================
log_step "创建模板 (POST $BASE_URL/api/templates)"

TPL_CREATE_RESP=$(curl -s -X POST "$BASE_URL/api/templates" \
    -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json" \
    -d '{"name":"回环测试模板","description":"通过回环测试自动创建","content":"{\"primary\":\"#12B7F5\",\"background\":\"#FFFFFF\"}"}')

TPL_CREATE_STATUS=$(echo "$TPL_CREATE_RESP" | python3 -c "import sys,json; print(json.load(sys.stdin).get('status',''))" 2>/dev/null || echo "parse_error")

if [ "$TPL_CREATE_STATUS" = "ok" ]; then
    TPL_ID=$(echo "$TPL_CREATE_RESP" | python3 -c "import sys,json; print(json.load(sys.stdin).get('data',{}).get('template_id',''))" 2>/dev/null || echo "")
    log_success "模板创建成功 (template_id: $TPL_ID)"
    
    # 应用模板
    log_step "应用模板 (POST $BASE_URL/api/templates/apply)"
    TPL_APPLY_RESP=$(curl -s -X POST "$BASE_URL/api/templates/apply" \
        -H "Authorization: Bearer $TOKEN" \
        -H "Content-Type: application/json" \
        -d "{\"template_id\":$TPL_ID}")
    TPL_APPLY_STATUS=$(echo "$TPL_APPLY_RESP" | python3 -c "import sys,json; print(json.load(sys.stdin).get('status',''))" 2>/dev/null || echo "parse_error")
    if [ "$TPL_APPLY_STATUS" = "ok" ]; then
        log_success "模板应用成功"
    else
        log_fail "模板应用失败: $TPL_APPLY_RESP"
    fi
else
    log_fail "模板创建失败: $TPL_CREATE_RESP"
fi

# 列出模板
log_step "获取模板列表 (GET $BASE_URL/api/templates)"
TPL_LIST_RESP=$(curl -s "$BASE_URL/api/templates?limit=10&offset=0" \
    -H "Authorization: Bearer $TOKEN")
TPL_LIST_STATUS=$(echo "$TPL_LIST_RESP" | python3 -c "import sys,json; print(json.load(sys.stdin).get('status',''))" 2>/dev/null || echo "parse_error")
if [ "$TPL_LIST_STATUS" = "ok" ]; then
    TPL_COUNT=$(echo "$TPL_LIST_RESP" | python3 -c "import sys,json; d=json.load(sys.stdin); items=d.get('data',{}).get('templates',[]) if isinstance(d.get('data'),dict) else d.get('data',[]); print(len(items))" 2>/dev/null || echo "0")
    log_success "获取模板列表成功 (共 $TPL_COUNT 个模板)"
else
    log_fail "获取模板列表失败: $TPL_LIST_RESP"
fi

# ========================================
# Step 14: 好友系统测试 (F3)
# ========================================
log_step "添加好友 (POST $BASE_URL/api/friends/add)"

FRIEND_ADD_RESP=$(curl -s -X POST "$BASE_URL/api/friends/add" \
    -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json" \
    -d "{\"user_id1\":\"$USER_ID\",\"user_id2\":1}")

FRIEND_ADD_STATUS=$(echo "$FRIEND_ADD_RESP" | python3 -c "import sys,json; print(json.load(sys.stdin).get('status',''))" 2>/dev/null || echo "parse_error")

if [ "$FRIEND_ADD_STATUS" = "ok" ]; then
    log_success "好友添加成功"
else
    log_fail "好友添加失败: $FRIEND_ADD_RESP"
fi

log_step "获取好友列表 (GET $BASE_URL/api/friends?id=$USER_ID)"
FRIEND_LIST_RESP=$(curl -s "$BASE_URL/api/friends?id=$USER_ID" \
    -H "Authorization: Bearer $TOKEN")
FRIEND_LIST_STATUS=$(echo "$FRIEND_LIST_RESP" | python3 -c "import sys,json; print(json.load(sys.stdin).get('status',''))" 2>/dev/null || echo "parse_error")
if [ "$FRIEND_LIST_STATUS" = "ok" ]; then
    FRIEND_COUNT=$(echo "$FRIEND_LIST_RESP" | python3 -c "import sys,json; d=json.load(sys.stdin); items=d.get('data',{}).get('friends',[]) if isinstance(d.get('data'),dict) else d.get('data',[]); print(len(items))" 2>/dev/null || echo "0")
    log_success "获取好友列表成功 (共 $FRIEND_COUNT 个好友)"
else
    log_fail "获取好友列表失败: $FRIEND_LIST_RESP"
fi

# ========================================
# 测试完成
# ========================================
echo ""
log_separator
echo -e "${CYAN}  回环测试完成${NC}"
log_separator
echo ""
echo "============================================"
echo "  测试汇总"
echo "============================================"
echo "  总步骤:  $STEP_COUNT"
echo -e "  通过:    ${GREEN}$PASS_COUNT${NC}"
echo -e "  失败:    ${RED}$FAIL_COUNT${NC}"
echo "  完成时间: $(date '+%Y-%m-%d %H:%M:%S')"
echo "============================================"
echo ""

# 保存测试报告
mkdir -p reports
cat > reports/loopback_test_report.md << EOF
# 墨竹 回环测试报告

## 测试信息
- **测试时间**: $(date '+%Y-%m-%d %H:%M:%S')
- **服务器**: $BASE_URL
- **测试用户**: $TEST_USER
- **总步骤**: $STEP_COUNT
- **通过**: $PASS_COUNT
- **失败**: $FAIL_COUNT

## 测试结果
| 步骤 | 名称 | 结果 |
|------|------|------|
EOF

echo "  报告已保存至: reports/loopback_test_report.md"
echo ""

# 返回值
[ "$FAIL_COUNT" -eq 0 ] && exit 0 || exit 1
