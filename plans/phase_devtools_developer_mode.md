# 开发者模式（DevTools）实施计划

## 概述

将 [`client/tools/debug_cli.c`](client/tools/debug_cli.c)（3093 行单体 C 文件）重构为应用内「开发者模式」——一个**独立的 CLI 工具** + **图形化调试界面** + **C++ 后端引擎**三位一体的开发者工具箱。

## 架构总览

```
client/
├── devtools/                          # 开发者模式根目录 ★ 新建
│   ├── CMakeLists.txt                 # 构建脚本 (独立编译 + 主 app 集成)
│   ├── README.md                      # 开发者模式文档
│   ├── cli/                           # 独立的 CLI 工具 (可单独编译运行)
│   │   ├── Makefile                   # 独立编译 (gcc)
│   │   ├── main.c                     # CLI 入口 + REPL 循环
│   │   ├── devtools_cli.h             # CLI 公共头文件
│   │   └── commands/                  # 命令模块 (从 debug_cli.c 拆分)
│   │       ├── cmd_health.c           # health 命令
│   │       ├── cmd_endpoint.c         # endpoint 测试
│   │       ├── cmd_token.c            # JWT 令牌分析
│   │       ├── cmd_user.c             # 用户管理
│   │       ├── cmd_ipc.c              # IPC 调试
│   │       ├── cmd_ws.c               # WebSocket 调试
│   │       ├── cmd_msg.c              # 消息操作
│   │       ├── cmd_friend.c           # 好友操作
│   │       ├── cmd_db.c               # 数据库操作
│   │       ├── cmd_session.c          # 会话管理
│   │       ├── cmd_config.c           # 配置管理
│   │       ├── cmd_storage.c          # 安全存储
│   │       ├── cmd_crypto.c           # 加密测试
│   │       ├── cmd_network.c          # 网络诊断
│   │       ├── cmd_ping.c             # Ping
│   │       ├── cmd_watch.c            # 实时监控
│   │       ├── cmd_rate_test.c        # 速率测试
│   │       ├── cmd_json.c             # JSON 工具
│   │       ├── cmd_tls.c              # TLS 信息
│   │       └── cmd_trace.c            # 请求追踪
│   ├── core/                          # C++ 后端引擎 (集成到主 app)
│   │   ├── DevToolsEngine.h           # 开发者模式引擎
│   │   ├── DevToolsEngine.cpp         # 生命周期管理 + 命令注册
│   │   ├── DevToolsHttpApi.h          # HTTP API 路由处理器
│   │   ├── DevToolsHttpApi.cpp        # /api/devtools/ 路由实现
│   │   └── DevToolsIpcHandler.h       # IPC 消息处理器
│   │   └── DevToolsIpcHandler.cpp     # 0xA0-0xEF 扩展消息处理
│   └── ui/                            # 图形化调试界面 (HTML/CSS/JS)
│       ├── devtools.html              # 开发者模式主页面
│       ├── css/
│       │   └── devtools.css           # 调试界面样式
│       └── js/
│           ├── devtools.js             # 主控制器 + 面板路由
│           ├── panel_api.js           # API 端点测试面板
│           ├── panel_network.js       # 网络连接监控面板
│           ├── panel_ws.js            # WebSocket 调试面板
│           ├── panel_ipc.js           # IPC 消息检查面板
│           ├── panel_storage.js       # 存储查看面板
│           ├── panel_crypto.js        # 加密测试面板
│           ├── panel_config.js        # 配置编辑面板
│           ├── panel_perf.js          # 性能测试面板 (集成 stress_test)
│           └── panel_logs.js          # 实时日志查看面板
├── tools/
│   ├── debug_cli.c                    # 保留原始文件作为参考 (或标记为已迁移)
│   └── stress_test.c                  # 保留，性能测试功能将集成到 panel_perf
```

## 执行步骤

### 步骤 1: 创建 `client/devtools/` 目录结构

创建目录层次，建立 `devtools/cli/commands/`、`devtools/core/`、`devtools/ui/js/`、`devtools/ui/css/`。

### 步骤 2: 重构 CLI — 拆分命令模块

从 [`debug_cli.c`](client/tools/debug_cli.c) 中将每个 `cmd_*` 函数提取到单独的文件：

| 原函数 | 目标文件 | 命令类别 |
|--------|---------|---------|
| [`cmd_health()`](client/tools/debug_cli.c:510) | `commands/cmd_health.c` | 基础功能 |
| [`cmd_endpoint()`](client/tools/debug_cli.c:552) | `commands/cmd_endpoint.c` | API 测试 |
| [`cmd_token()`](client/tools/debug_cli.c:603) | `commands/cmd_token.c` | JWT 分析 |
| [`cmd_user()`](client/tools/debug_cli.c:849) + 子命令 | `commands/cmd_user.c` | 用户管理 |
| [`cmd_ipc()`](client/tools/debug_cli.c:916) | `commands/cmd_ipc.c` | IPC 调试 |
| [`cmd_ws()`](client/tools/debug_cli.c:1829) | `commands/cmd_ws.c` | WebSocket |
| [`cmd_msg()`](client/tools/debug_cli.c:1998) | `commands/cmd_msg.c` | 消息操作 |
| [`cmd_friend()`](client/tools/debug_cli.c:2097) | `commands/cmd_friend.c` | 好友操作 |
| [`cmd_db()`](client/tools/debug_cli.c:2180) | `commands/cmd_db.c` | 数据库 |
| [`cmd_session()`](client/tools/debug_cli.c:2246) | `commands/cmd_session.c` | 会话管理 |
| [`cmd_config()`](client/tools/debug_cli.c:2311) | `commands/cmd_config.c` | 配置管理 |
| [`cmd_storage()`](client/tools/debug_cli.c:2388) | `commands/cmd_storage.c` | 安全存储 |
| [`cmd_crypto()`](client/tools/debug_cli.c:2464) | `commands/cmd_crypto.c` | 加密测试 |
| [`cmd_network()`](client/tools/debug_cli.c:2635) | `commands/cmd_network.c` | 网络诊断 |
| [`cmd_ping()`](client/tools/debug_cli.c:1240) | `commands/cmd_ping.c` | 性能测试 |
| [`cmd_watch()`](client/tools/debug_cli.c:1305) | `commands/cmd_watch.c` | 实时监控 |
| [`cmd_rate_test()`](client/tools/debug_cli.c:1396) | `commands/cmd_rate_test.c` | 速率测试 |
| [`cmd_json_parse()`](client/tools/debug_cli.c:1093) + `cmd_json_pretty()` | `commands/cmd_json.c` | JSON 工具 |
| [`cmd_tls_info()`](client/tools/debug_cli.c:1065) | `commands/cmd_tls.c` | TLS 信息 |
| [`cmd_trace()`](client/tools/debug_cli.c:1175) | `commands/cmd_trace.c` | 请求追踪 |

每个命令文件导出统一的接口：

```c
// devtools_cli.h 中定义
typedef int (*CommandHandler)(int argc, char** argv);

typedef struct {
    const char* name;          // 命令名称
    const char* description;   // 简短描述
    const char* usage;         // 用法
    CommandHandler handler;    // 处理函数
} CommandEntry;

// 每个 cmd_*.c 导出
const CommandEntry* get_command_entry(void);
```

### 步骤 3: 创建 CLI 主入口 (`cli/main.c`)

- REPL 循环（保留 `debug_cli.c` 的交互模式）
- 命令注册表（从所有 `cmd_*.c` 注册）
- 全局状态（从 `g_config` 迁移，保留独立运行能力）
- `help` / `exit` / `verbose`

### 步骤 4: 创建 C++ 后端引擎 (`core/DevToolsEngine`)

利用 [`ClientHttpServer`](client/src/app/ClientHttpServer.h:56) 已预留的 `kDevToolRoutePrefix = "/api/devtools/"` 路由前缀：

```cpp
// DevToolsEngine.h
class DevToolsEngine {
public:
    bool init(AppContext& ctx);
    void shutdown();
    
    // 注册 HTTP API 路由到 ClientHttpServer
    void register_http_routes(ClientHttpServer& http_server);
    
    // 注册 IPC 处理器到 IpcBridge
    void register_ipc_handlers(IpcBridge& ipc);
    
    // 执行 CLI 命令 (从 JS/HTTP 调用)
    std::string execute_command(const std::string& cmd_line);
    
    // 状态查询
    bool is_dev_mode_enabled() const;
    void set_dev_mode_enabled(bool enabled);

private:
    bool enabled_ = false;
    AppContext* ctx_ = nullptr;
    std::vector<std::unique_ptr<CommandModule>> modules_;
};
```

### 步骤 5: 实现 HTTP API 路由 (`core/DevToolsHttpApi`)

通过 [`ClientHttpServer::register_route()`](client/src/app/ClientHttpServer.cpp:333) 注册以下端点：

| 路由 | 方法 | 功能 |
|------|------|------|
| `POST /api/devtools/exec` | POST | 执行 CLI 命令并返回 JSON 结果 |
| `GET /api/devtools/commands` | GET | 列出所有可用命令 |
| `GET /api/devtools/status` | GET | 获取开发者模式状态 |
| `POST /api/devtools/enable` | POST | 启用开发者模式 |
| `POST /api/devtools/disable` | POST | 禁用开发者模式 |
| `GET /api/devtools/network/status` | GET | 网络连接状态 |
| `GET /api/devtools/storage/list` | GET | 存储内容列表 |
| `POST /api/devtools/endpoint/test` | POST | 测试 API 端点 |
| `GET /api/devtools/ws/status` | GET | WebSocket 状态 |
| `POST /api/devtools/ws/send` | POST | 发送 WebSocket 消息 |

响应格式统一为 JSON：

```json
{
    "status": "ok",
    "data": { ... },
    "timestamp": 1234567890
}
```

### 步骤 6: 实现 IPC 处理器 (`core/DevToolsIpcHandler`)

通过 [`IpcBridge::register_handler()`](client/src/app/IpcBridge.cpp:29) 注册 `kExtensionBase`-`kExtensionMax` 范围内的消息类型：

| IPC 消息类型 | 方向 | 功能 |
|-------------|------|------|
| `0xC0` (DEV_TOOLS_ENABLE) | JS → C++ | JS 面板启用开发者模式 |
| `0xC1` (DEV_TOOLS_EXEC) | JS → C++ | 执行 CLI 命令 |
| `0xC2` (DEV_TOOLS_LOG) | C++ → JS | 推送日志到 UI 面板 |
| `0xC3` (DEV_TOOLS_NETWORK_EVENT) | C++ → JS | 推送网络事件 |

### 步骤 7: 创建前端主页面 (`ui/devtools.html`)

- 由 [`index.html`](client/ui/index.html) 的「开发者模式」按键触发加载
- 可通过 `iframe` 嵌入主 app，或通过 WebView2 导航栏切换
- 左侧菜单栏列出所有调试面板
- 右侧内容区显示对应面板

### 步骤 8: 创建图形化调试面板

每个面板对应一个 `panel_*.js`：

#### `panel_api.js` — API 端点测试
```
+--------------------------------------------------+
| [Method ▼] [Endpoint URL                ] [Send] |
+--------------------------------------------------+
| Headers:                                          |
|   Key: [____________]  Value: [________] [+]    |
+--------------------------------------------------+
| Body (JSON):                                      |
|   { ... }                                         |
+--------------------------------------------------+
| Response:                                         |
|   Status: 200 OK                                  |
|   Time: 45ms                                      |
|   { "result": "ok" }                              |
+--------------------------------------------------+
| [Save as Test] [History]                          |
+--------------------------------------------------+
```

#### `panel_ws.js` — WebSocket 调试
```
+--------------------------------------------------+
| Connect | [Token: ______] [Path: ______] [Connect]|
+--------------------------------------------------+
| Messages:                                         |
|  [12:34:56] → { "type": "text", ... }            |
|  [12:34:57] ← { "type": "ack", ... }             |
+--------------------------------------------------+
| Send: [________________] [Send]                   |
+--------------------------------------------------+
| Status: ● Connected  Pings: 5  Latency: 12ms     |
+--------------------------------------------------+
```

#### `panel_ipc.js` — IPC 消息检查器
```
+--------------------------------------------------+
| [Start Capture] [Clear] [Filter: 0x__ ▼]         |
+--------------------------------------------------+
| # | Timestamp | Type | Direction | Data           |
|---|-----------|------|-----------|----------------|
| 1 | 12:34:56  | 0x01 | JS → C++  | { login:.. }  |
| 2 | 12:34:57  | 0x10 | JS → C++  | { msg:.. }    |
+--------------------------------------------------+
```

#### `panel_network.js` — 网络连接监控
```
+--------------------------------------------------+
| Server: 127.0.0.1:4443                           |
| Status: ● Connected                               |
| Uptime: 2h 34m                                    |
| Sent: 1.2 MB  |  Recv: 3.4 MB                     |
| Latency: 15ms (avg)                               |
+--------------------------------------------------+
| [Test Connection] [TLS Info] [Trace Route]        |
+--------------------------------------------------+
```

#### `panel_config.js` — 配置编辑器
```
+--------------------------------------------------+
| Config Key     | Value                | Action    |
|----------------|----------------------|-----------|
| server_host    | 127.0.0.1          | [Edit]    |
| server_port    | 4443                | [Edit]    |
| auto_reconnect | true                | [Toggle]  |
| log_level      | Info                | [▼]       |
+--------------------------------------------------+
| [Reload Config] [Save Config] [Reset Defaults]   |
+--------------------------------------------------+
```

#### 其他面板：`panel_storage.js`、`panel_crypto.js`、`panel_perf.js`、`panel_logs.js`

按类似模式实现。

### 步骤 9: 创建 `DevToolsEngine` 管理状态和生命周期

```cpp
// DevToolsEngine.cpp 核心逻辑

bool DevToolsEngine::init(AppContext& ctx) {
    ctx_ = &ctx;
    
    // 注册 HTTP 路由
    register_http_routes(ctx.http_server());
    
    // 注册 IPC 处理器
    register_ipc_handlers(ctx.ipc());
    
    // 从配置加载启用状态
    std::string dev_mode_val;
    if (ctx.storage().load_config("dev_mode_enabled", dev_mode_val) == 0) {
        enabled_ = (dev_mode_val == "true");
    }
    
    LOG_INFO("开发者模式引擎初始化完成 (启用=%s)", enabled_ ? "是" : "否");
    return true;
}

void DevToolsEngine::register_http_routes(ClientHttpServer& server) {
    // 注册 /api/devtools/exec 路由
    server.register_route("/api/devtools/exec", 
        [this](SOCKET fd, const std::string& path, 
               const std::string& method, const std::string& body) {
            // 解析 JSON 获取命令
            // 执行命令
            // 返回 JSON 结果
        });
    
    // 注册其他路由...
}

void DevToolsEngine::register_ipc_handlers(IpcBridge& ipc) {
    // 注册 IPC 0xC1 (执行命令)
    ipc.register_handler(
        static_cast<IpcMessageType>(0xC1),
        [this](IpcMessageType type, const std::string& json_data) {
            // 执行命令并通过 IPC 返回结果
        });
}
```

### 步骤 10: 集成到 AppContext

在 [`AppContext`](client/src/app/AppContext.h) 中添加开发者模式引擎：

```cpp
// AppContext.h 新增成员
#include "../devtools/core/DevToolsEngine.h"

class AppContext {
    // ... 现有成员 ...
    std::unique_ptr<DevToolsEngine> devtools_;  // ★ 新增
};

// AppContext.cpp 构造函数
AppContext::AppContext()
    : // ... 现有初始化 ...
    , devtools_(std::make_unique<DevToolsEngine>())
{
}

// AppContext::init() 新增
int AppContext::init(const ClientConfig& config) {
    // ... 现有初始化顺序 ...
    
    // 8. 初始化开发者模式引擎
    if (devtools_->init(*this) != 0) {
        LOG_WARN("开发者模式引擎初始化失败");
    }
}
```

### 步骤 11: 前端集成 — 添加「开发者模式」入口

在 [`index.html`](client/ui/index.html) 的设置页面添加开发者模式切换：

```html
<!-- 在 settings-section 中添加 -->
<div class="settings-section">
    <h3>🔧 开发者模式</h3>
    <div class="form-group">
        <label>启用开发者模式</label>
        <input type="checkbox" id="dev-mode-toggle" onchange="toggleDevMode()">
    </div>
    <div class="form-group" id="dev-mode-panel" style="display:none;">
        <button onclick="openDevTools()">打开开发者工具</button>
    </div>
</div>
```

在 [`app.js`](client/ui/js/app.js) 中添加控制逻辑：

```javascript
function toggleDevMode() {
    const enabled = document.getElementById('dev-mode-toggle').checked;
    document.getElementById('dev-mode-panel').style.display = enabled ? 'block' : 'none';
    // 通过 IPC 通知后端
    IPC.send(0xC0, { enabled: enabled });
}

function openDevTools() {
    // 通过 HTTP API 打开 devtools 页面
    window.open('http://127.0.0.1:9010/api/devtools/ui/devtools.html', '_blank');
}
```

### 步骤 12: 更新 CMakeLists.txt

在 [`client/CMakeLists.txt`](client/CMakeLists.txt) 中添加 devtools 源文件：

```cmake
# 在 CLIENT_CPP_SOURCES 中添加
file(GLOB_RECURSE CLIENT_CPP_SOURCES
    src/util/*.cpp
    src/network/*.cpp
    src/app/*.cpp
    src/storage/*.cpp
    src/security/*.cpp
    src/ai/*.cpp
    devtools/core/*.cpp          # ★ 新增
)

# 添加 include 路径
target_include_directories(chrono-client PRIVATE
    include
    src
    devtools                     # ★ 新增
)
```

同时创建 [`devtools/CMakeLists.txt`](client/devtools/CMakeLists.txt) 用于独立编译 CLI + UI：

```cmake
# 独立的 CLI 构建
add_executable(devtools-cli
    cli/main.c
    cli/commands/cmd_health.c
    cli/commands/cmd_endpoint.c
    # ... 所有命令文件
)
```

### 步骤 13: 更新 index.html 脚本加载

在 [`index.html`](client/ui/index.html) 的脚本加载部分添加 devtools 脚本（条件加载）：

```html
<!-- 在 ai_chat.js 之后 -->
<script src="js/devtools/devtools.js"></script>
```

### 步骤 14: 为 CLI 创建独立 Makefile

在 [`devtools/cli/Makefile`](client/devtools/cli/Makefile) 中提供独立编译支持（与现有 [`client/tools/Makefile`](client/tools/Makefile) 保持兼容）：

```makefile
CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -I./include -I../../include
LDFLAGS = -lws2_32 -lcrypt32

SRCS = main.c $(wildcard commands/*.c)
OBJS = $(SRCS:.c=.o)

devtools-cli: $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<
```

### 步骤 15: 功能对应表 — debug_cli.c → 新架构

| debug_cli.c 功能 | CLI 命令 | HTTP API | UI 面板 | 状态 |
|-----------------|----------|----------|---------|------|
| `health` | `cmd_health.c` | `GET /api/devtools/health` | 状态栏 | 待迁移 |
| `endpoint` | `cmd_endpoint.c` | `POST /api/devtools/endpoint/test` | `panel_api.js` | 待迁移 |
| `token` | `cmd_token.c` | `POST /api/devtools/token/decode` | `panel_api.js` | 待迁移 |
| `user` | `cmd_user.c` | — | — | 待迁移 |
| `ipc` | `cmd_ipc.c` | `GET /api/devtools/ipc/status` | `panel_ipc.js` | 待迁移 |
| `ws` | `cmd_ws.c` | — | `panel_ws.js` | 待迁移 |
| `msg` | `cmd_msg.c` | — | — | 待迁移 |
| `friend` | `cmd_friend.c` | — | — | 待迁移 |
| `db` | `cmd_db.c` | — | — | 待迁移 |
| `session` | `cmd_session.c` | — | — | 待迁移 |
| `config` | `cmd_config.c` | `POST /api/devtools/config/set` | `panel_config.js` | 待迁移 |
| `storage` | `cmd_storage.c` | `GET /api/devtools/storage/list` | `panel_storage.js` | 待迁移 |
| `crypto` | `cmd_crypto.c` | — | `panel_crypto.js` | 待迁移 |
| `network` | `cmd_network.c` | `GET /api/devtools/network/status` | `panel_network.js` | 待迁移 |
| `ping` | `cmd_ping.c` | — | `panel_perf.js` | 待迁移 |
| `watch` | `cmd_watch.c` | — | `panel_perf.js` | 待迁移 |
| `rate-test` | `cmd_rate_test.c` | — | `panel_perf.js` | 待迁移 |
| `json-parse` | `cmd_json.c` | — | `panel_api.js` | 待迁移 |
| `json-pretty` | `cmd_json.c` | — | `panel_api.js` | 待迁移 |
| `tls-info` | `cmd_tls.c` | `GET /api/devtools/tls/info` | `panel_network.js` | 待迁移 |
| `trace` | `cmd_trace.c` | — | `panel_network.js` | 待迁移 |
| `connect` | `cli/main.c` | — | — | 待迁移 |
| `disconnect` | `cli/main.c` | — | — | 待迁移 |
| `verbose` | `cli/main.c` | — | — | 待迁移 |
| `help` | `cli/main.c` | `GET /api/devtools/commands` | — | 待迁移 |

### 步骤 16: 添加开发者模式 CSS 样式

在 [`devtools/ui/css/devtools.css`](client/devtools/ui/css/devtools.css) 中：

```css
:root {
    --dt-bg: #1e1e2e;
    --dt-surface: #2a2a3e;
    --dt-primary: #89b4fa;
    --dt-success: #a6e3a1;
    --dt-warning: #f9e2af;
    --dt-error: #f38ba8;
    --dt-text: #cdd6f4;
    --dt-text-dim: #a6adc8;
}

.devtools-container { ... }
.devtools-sidebar { ... }
.devtools-panel { ... }
/* ... 更多样式 ... */
```

### 步骤 17: 测试与验证

1. **CLI 独立编译测试**：`cd devtools/cli && make && ./devtools-cli`
2. **主 app 编译测试**：确保 CMakeLists.txt 包含 devtools/core/*.cpp
3. **HTTP API 测试**：`curl http://127.0.0.1:9010/api/devtools/commands`
4. **UI 面板测试**：在浏览器中打开 devtools.html
5. **IPC 通信测试**：从 JS 面板发送 IPC 消息，验证 C++ 端接收

## 依赖关系图

```
┌─────────────────────────────────────────────────┐
│                 index.html                       │
│  [设置] → [开发者模式开关]                        │
└──────────────────┬──────────────────────────────┘
                   │ IPC (0xC0) / HTTP
                   ▼
┌─────────────────────────────────────────────────┐
│              DevToolsEngine (C++)                │
│  ┌──────────┐  ┌──────────────┐  ┌───────────┐  │
│  │ HTTP API │  │ IPC Handler  │  │ Cmd Exec  │  │
│  │ /api/    │  │ 0xC0-0xC3   │  │ Engine    │  │
│  │ devtools/│  │              │  │           │  │
│  └────┬─────┘  └──────┬───────┘  └─────┬─────┘  │
│       │               │                │         │
└───────┼───────────────┼────────────────┼─────────┘
        │               │                │
        ▼               ▼                ▼
┌───────────┐  ┌──────────────┐  ┌────────────────┐
│ AppContext│  │ IpcBridge    │  │ debug_cli.c     │
│ (network, │  │ (0xA0-0xEF  │  │ (命令实现)       │
│  storage, │  │  扩展范围)   │  │ → 拆分到        │
│  session, │  │              │  │ cli/commands/   │
│  crypto…) │  │              │  │                  │
└───────────┘  └──────────────┘  └────────────────┘
                                           │
                                           ▼
                                  ┌────────────────┐
                                  │ devtools/ui/   │
                                  │ HTML/CSS/JS    │
                                  │ 调试面板        │
                                  └────────────────┘
```

## 风险评估

| 风险 | 概率 | 影响 | 缓解方案 |
|------|------|------|---------|
| debug_cli.c 依赖 OpenSSL (crypto) | 低 | 高 | CLI 独立编译时可选链接，核心引擎使用 AppContext::crypto() |
| WebView2 加载 iframe 安全限制 | 中 | 中 | 使用同源策略，通过 127.0.0.1:9010 加载 |
| IPC 消息冲突 (0xC0 范围) | 低 | 中 | 使用文档中定义的专用范围，在 ipc.js 中添加注释 |
| CMakeLists.txt 增加编译时间 | 低 | 低 | 只在有 devtools 目录时 glob，不影响正常编译 |

## 文件变更清单

### 新增文件 (共 ~40 个)

- `client/devtools/CMakeLists.txt`
- `client/devtools/README.md`
- `client/devtools/cli/Makefile`
- `client/devtools/cli/main.c`
- `client/devtools/cli/devtools_cli.h`
- `client/devtools/cli/commands/cmd_*.c` (×20)
- `client/devtools/core/DevToolsEngine.h`
- `client/devtools/core/DevToolsEngine.cpp`
- `client/devtools/core/DevToolsHttpApi.h`
- `client/devtools/core/DevToolsHttpApi.cpp`
- `client/devtools/core/DevToolsIpcHandler.h`
- `client/devtools/core/DevToolsIpcHandler.cpp`
- `client/devtools/ui/devtools.html`
- `client/devtools/ui/css/devtools.css`
- `client/devtools/ui/js/devtools.js`
- `client/devtools/ui/js/panel_*.js` (×9)

### 修改文件 (共 5 个)

- `client/src/app/AppContext.h` — 添加 DevToolsEngine 成员
- `client/src/app/AppContext.cpp` — 初始化 DevToolsEngine
- `client/CMakeLists.txt` — 添加 devtools/core 源文件
- `client/ui/index.html` — 添加开发者模式 UI 入口
- `client/ui/js/app.js` — 添加 toggleDevMode() 和 openDevTools()

### 保留文件 (不修改)

- `client/tools/debug_cli.c` — 保留作为参考
- `client/tools/stress_test.c` — 保留，性能数据通过 panel_perf.js 展示
- `client/tools/Makefile` — 保留
