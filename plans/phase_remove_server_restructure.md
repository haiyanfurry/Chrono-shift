# 删除 server/ 目录 + 客户端重构 & CLI 扩展计划

## 一、背景分析

### 客户端对 server/ 的依赖（删除前必须处理）

| 依赖项 | 涉及文件 | 处理方式 |
|--------|---------|---------|
| `server/include/tls_server.h` (TLS 抽象层) | `client/src/network/*.cpp` (4个文件) | 移动到 `client/include/tls_client.h` |
| `server/include/platform_compat.h` | `client/src/network/*.cpp` | 移动到 `client/include/` |
| `server/include/json_parser.h` | `client/src/*.c` | 移动到 `client/include/` |
| `server/security/` (Rust FFI模块) | 原本给 server 用，不依赖 | 直接删除（客户端有自己的 `client/security/`） |
| `server/include/` 其他头文件 | `client/include/net_core.h` 等间接引用 | 按需迁移 |

### 现有 CLI 工具

1. **`server/tools/debug_cli.c`** (2318行) — 主要 CLI，支持：
   - 连接管理: `connect`, `disconnect`
   - API测试: `health`, `endpoint`, `token`
   - IPC通信: `ipc send`, `ipc types`
   - 用户管理: `user list/get/create/delete`
   - WebSocket: `ws connect/send/recv/close/status`
   - 消息&好友: `msg list/get/send`, `friend list/add`
   - 数据库: `db list`
   - 诊断: `tls-info`, `json-parse`, `json-pretty`
   - 性能: `trace`, `ping`, `watch`, `rate-test`

2. **`server/tools/stress_test.c`** (777行) — 压力测试工具

---

## 二、执行步骤

### 步骤 1：迁移共享头文件

**目标**：将 client 依赖的 server 头文件移动到 client 目录

| 动作 | 源路径 | 目标路径 |
|------|--------|---------|
| 移动 | `server/include/tls_server.h` | `client/include/tls_client.h` |
| 移动 | `server/include/platform_compat.h` | `client/include/platform_compat.h` |
| 移动 | `server/include/json_parser.h` | `client/include/json_parser.h` |
| 移动 | `server/include/protocol.h` | `client/include/protocol.h` |
| 移动 | `server/src/tls_server.c` | `client/src/network/tls_client.c` |

**注意**：`tls_server.h` 需要重命名为 `tls_client.h`，并清理服务端 API（只保留客户端 API：`tls_client_init`, `tls_client_connect`, `tls_client_cleanup`, `tls_read`, `tls_write`, `tls_get_info`, `tls_last_error`）

### 步骤 2：更新客户端 include 引用

**文件**：`client/src/network/*.cpp` (4个文件)

```
// 修改前:
#include "../../server/include/tls_server.h"
// 修改后:
#include "../include/tls_client.h"
```

### 步骤 3：更新 CMakeLists.txt

- **`client/CMakeLists.txt`**: 移除 `../server/include` 路径，改为 `include` 目录
- **`CMakeLists.txt`**: 移除 `add_subdirectory(server)`
- **`Makefile`**: 移除 server 相关 target

### 步骤 4：迁移 CLI 工具

- 创建 `client/tools/` 目录
- 复制 `server/tools/debug_cli.c` → `client/tools/debug_cli.c`
- 复制 `server/tools/stress_test.c` → `client/tools/stress_test.c`
- 更新 CLI 中的 include 路径（从 `../include/` 改为 `../include/` 或 `../../client/include/`）

### 步骤 5：扩展 CLI 命令（新功能）

在现有 CLI 基础上添加以下客户端相关命令：

```
客户端本地命令:
  - session show          : 查看当前会话状态
  - session login <host> <token> : 模拟登录
  - session logout        : 清除会话
  - config show           : 查看客户端配置
  - config set <key> <value> : 修改配置项
  - storage list          : 列出本地安全存储内容
  - storage get <key>     : 读取安全存储条目
  - crypto test           : 测试 E2E 加密/解密
  - network test <host> <port> : 网络连通性测试
  - ws monitor            : WebSocket 连接监控模式
  - ipc capture           : 捕获 IPC 消息（监听模式）
```

### 步骤 6：更新文档 & 安装脚本

- `docs/BUILD.md`：删除服务器构建说明，聚焦客户端
- `installer/client_installer.nsi`：更新路径引用
- `README.md`：更新项目描述

### 步骤 7：删除 server/ 目录

```
git rm -r server/
```

### 步骤 8：清理 & 验证

- 运行 `cargo build` 验证 `client/security/` Rust 模块可正常编译
- 验证 CLI 工具可编译
- 验证客户端可正常构建

---

## 三、新项目结构

```
chrono-shift/
├── client/                    # 客户端主项目
│   ├── include/               # 头文件（含从 server 迁入的）
│   │   ├── tls_client.h       # TLS 客户端 API
│   │   ├── platform_compat.h  # 平台兼容层
│   │   ├── json_parser.h      # JSON 解析器
│   │   ├── protocol.h         # 通信协议
│   │   ├── net_core.h         # 网络核心
│   │   └── ... (原有)
│   ├── src/                   # C/C++ 源码
│   │   ├── app/               # 应用层 (Main, IPC, WebView)
│   │   ├── network/           # 网络层 (TCP, TLS, WebSocket)
│   │   ├── security/          # 安全模块 (加密, 令牌)
│   │   ├── storage/           # 本地存储
│   │   ├── util/              # 工具函数
│   │   └── *.c (原有C代码)
│   ├── tools/                 # ★ 新目录 - CLI 工具
│   │   ├── debug_cli.c        # ★ 主 CLI（扩展后）
│   │   └── stress_test.c      # 压力测试工具
│   ├── security/              # Rust 安全模块 (已有)
│   ├── ui/                    # 前端 (HTML/CSS/JS)
│   └── CMakeLists.txt
├── installer/                 # NSIS 安装脚本
├── docs/                      # 文档
├── plans/                     # 计划文件
├── tests/                     # 测试脚本
└── CMakeLists.txt             # 仅 build client
```
