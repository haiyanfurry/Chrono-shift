# Phase 7 — 代码优化与重构计划

> **版本**: v0.2.0 → v0.3.0  
> **目标**: 模块拆分、安全审计、CI/CD、单元测试、构建系统现代化  
> **优先级**: 按用户指定优先级从高到低

---

## 目录

- [P1: 巨型源文件模块拆分](#p1-巨型源文件模块拆分)
- [P2: 内存与安全审计](#p2-内存与安全审计)
- [P3: 自动化 CI 与静态分析](#p3-自动化-ci-与静态分析)
- [P4: 单元测试与模糊测试](#p4-单元测试与模糊测试)
- [P5: 并发模型与错误处理](#p5-并发模型与错误处理)
- [P6: FFI 与 TLS 审计](#p6-ffi-与-tls-审计)
- [P7: 构建系统与可移植性](#p7-构建系统与可移植性)
- [P8: 代码质量改进](#p8-代码质量改进)
- [项目结构最终形态](#项目结构最终形态)
- [实施路线图](#实施路线图)

---

## P1: 巨型源文件模块拆分

### 当前状态 (按行数排序)

| 源文件 | 行数 | 函数数 | 职责 |
|--------|------|--------|------|
| `server/src/database.c` | ~1290 | 30+ | 全部 DB 操作揉在一个文件 |
| `server/src/http_server.c` | ~911 | 20+ | 事件循环/连接管理/HTTP 解析/路由/响应 |
| `client/src/network.c` | ~909 | 20+ | HTTP 客户端/WebSocket/SHA-1/连接管理 |
| `server/src/file_handler.c` | ~738 | 15+ | 上传/下载/头像/静态文件/MIME |
| `server/src/json_parser.c` | ~642 | 10+ | 手写 JSON 解析器 |
| `server/include/platform_compat.h` | ~383 | 15+ inline | 6 组不同职责的跨平台抽象(头文件巨集) |

### 1.1 database.c → 6 文件拆分

**拆分方案** (保留 `database.h` 公共接口不变):

```
server/src/
├── db_core.c          # 初始化/关闭/路径构建/ID分配/文件I/O
├── db_users.c         # 用户 CRUD (create/get/update/search)
├── db_messages.c      # 消息 CRUD (save/get/mark_read/sort)
├── db_friends.c       # 好友关系 (add/remove/list/check)
├── db_templates.c     # 模板 CRUD (create/get/apply/increment)
└── database.c         # (删除或转为包裹转发)
```

**接口头文件**:
- `server/include/db_core.h` — `db_init()`, `db_close()`, `allocate_id()`, `read_file_content()`
- `server/include/db_users.h` — 用户操作接口
- `server/include/db_messages.h` — 消息操作接口
- `server/include/db_friends.h` — 好友操作接口
- `server/include/db_templates.h` — 模板操作接口
- `server/include/database.h` — (保留为统一入口，或删除让各模块直接引用子头文件)

**关键变更**:
- 内部辅助函数 (`get_user_path`, `build_user_json` 等) 移入对应子模块的 `.c` 文件，标记 `static`
- 共享的 `g_db_base` 全局变量移到 `db_core.c`，提供 `db_get_base_path()` 访问器
- 共享的 `read_file_content()`, `file_exists()`, `ensure_dir()` 留在 `db_core.c`

### 1.2 http_server.c → 5 文件拆分

```
server/src/
├── http_core.c        # 服务器生命周期 (init/start/stop) + 事件循环
├── http_conn.c        # 连接管理 (create/close/list/timeout)
├── http_parser.c      # HTTP 请求解析 (parse_http_request)
├── http_route.c       # 路由表 (register_route/find_route)
└── http_response.c    # 响应构建 (build_http_response + response helper API)
```

**接口头文件** (新增):
- `server/include/http_conn.h` — `Connection` 结构体定义、连接池操作
- `server/include/http_route.h` — 路由条目、注册/查找
- `server/include/http_parser.h` — 请求解析状态机
- `server/include/http_response.h` — 响应构建 API

**保留 `server/include/http_server.h`** 作为统一公共接口。

**关键变更**:
- `event_loop()` 中的 accept/read/write 逻辑分发到各模块
- `parse_http_request()` 状态机独立，支持单元测试
- `Connection` 结构体定义迁移到 `http_conn.h`

### 1.3 client/src/network.c → 3 文件拆分

```
client/src/
├── net_http.c         # HTTP 客户端 (request/response)
├── net_ws.c           # WebSocket 客户端 (connect/send/recv/close)
├── net_sha1.c         # SHA-1 实现 (独立出来，也可被其他模块复用)
└── network.c          # (删除，或做转发)
```

**接口头文件** (新增):
- `client/include/net_http.h`
- `client/include/net_ws.h`
- `client/include/net_sha1.h`

**保留 `client/include/network.h`** 作为统一入口。

### 1.4 platform_compat.h → 1 header + 4 .c 文件

```
server/include/platform_compat.h   # 仅保留: 类型定义 + 宏 + 函数原型
server/src/
├── platform_compat.c  # 公共实现 (path_normalize/path_join/now_ms)
├── platform_dir.c     # 目录迭代 (dir_open/dir_next/dir_close)
├── platform_thread.c  # 线程/互斥体 (thread_create/join/mutex_*)
└── platform_net.c     # 网络初始化 (net_init/net_cleanup/set_nonblocking/set_blocking)
```

**关键变更**:
- 所有 `static inline` 函数 → 普通函数移到 `.c` 文件
- Header 中只保留 `#define` 宏、`typedef`、函数 `extern` 声明
- 减少每个包含此头文件的编译单元的代码膨胀
- Windows/Linux 平台差异实现通过条件编译在各自 `.c` 文件中处理

### 1.5 json_parser.c (不移除，添加测试)

- 维持单文件，因为 JSON 解析器是独立职责
- 添加单元测试文件 `tests/unit/test_json_parser.c`
- 添加模糊测试入口 `tests/fuzz/fuzz_json_parser.c`

---

## P2: 内存与安全审计

### 审计目标文件 (按风险优先级)

| 优先级 | 文件 | 风险点 |
|--------|------|--------|
| 🔴 高 | `json_parser.c` | 手写解析器，输入不可信，缓冲区操作密集 |
| 🔴 高 | `websocket.c` | 网络帧解析，掩码处理，长度校验 |
| 🔴 高 | `http_server.c` → `http_parser.c` | HTTP 请求解析，头部分隔 |
| 🟡 中 | `tls_server.c` | OpenSSL 调用，内存管理 |
| 🟡 中 | `rust_stubs.c` | FFI 边界，C↔Rust 内存所有权 |
| 🟡 中 | `file_handler.c` | 路径拼接，文件写入 |
| 🟢 低 | `protocol.c` | 协议编码，但代码简短 |

### 审计检查清单

```
□ 所有 snprintf() 返回值检查 (截断检测)
□ 所有 malloc/calloc → NULL 检查
□ 所有 memcpy/memmove → 长度越界检查
□ JSON 解析器: 嵌套深度限制 (≥ 32 层才拒绝)
□ JSON 解析器: 字符串长度限制 (≥ 64KB 才拒绝)
□ JSON 解析器: 键名重复检测
□ WebSocket: 帧长度验证 (≤ 最大帧大小)
□ WebSocket: 掩码键正确应用于客户端帧
□ HTTP: 请求行/头部分隔验证
□ HTTP: Content-Length 验证 (≤ 最大请求体)
□ FFI: Rust 返回的字符串谁释放？
□ FFI: C 传入 Rust 的指针生命周期
□ TLS: SSL_* 返回值检查
□ 文件: 路径规范化后的前缀检查 (防止 chroot 绕过)
```

### Sanitizer 构建配置

```cmake
# CMake 中新增 DebugSan 配置
set(CMAKE_C_FLAGS_DEBUGSAN
    "-g -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer"
    CACHE STRING "Flags for Debug with Sanitizers")
```

### 审计后修复流程

1. 每发现一个问题，新建 `fix-{module}-{issue}` 分支
2. 修复后提交，附带单元测试验证
3. CI 中通过 ASAN 运行测试确认修复

---

## P3: 自动化 CI 与静态分析

### GitHub Actions 工作流: `.github/workflows/ci.yml`

```yaml
name: CI
on: [push, pull_request]

jobs:
  build:
    strategy:
      matrix:
        os: [ubuntu-latest, windows-latest]
        config: [Debug, Release, DebugSan]
    runs-on: ${{ matrix.os }}
    steps:
      - uses: actions/checkout@v4
      - name: Install Dependencies
        run: ...  # apt install / choco install
      - name: Build
        run: cmake -B build -DCMAKE_BUILD_TYPE=${{ matrix.config }} && cmake --build build
      - name: Run Tests
        run: ctest --test-dir build --output-on-failure

  static-analysis:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: clang-tidy
        run: cmake -B build -DCMAKE_C_CLANG_TIDY="clang-tidy;--checks=*" && cmake --build build
      - name: cppcheck
        run: cppcheck --enable=all --suppress=missingIncludeSystem server/src/
      - name: Rust Clippy
        run: cd server/security && cargo clippy -- -D warnings

  formatting:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: clang-format check
        run: find server/src client/src -name '*.c' -o -name '*.h' | xargs clang-format --dry-run --Werror
      - name: rustfmt check
        run: cd server/security && cargo fmt --check
```

### 静态分析工具集成

| 工具 | 用途 | 配置 |
|------|------|------|
| `clang-tidy` | C 代码静态分析 | `.clang-tidy` 配置文件 |
| `cppcheck` | 额外 C 静态分析 | `--enable=all --suppress=missingIncludeSystem` |
| `scan-build` | Clang 静态分析器 | 可选 CI job |
| `cargo clippy` | Rust 代码检查 | `-- -D warnings` (拒绝警告) |
| `cargo fmt` | Rust 格式化 | CI 中 `--check` |

### `.clang-tidy` 配置

```yaml
Checks: >
  clang-analyzer-*,
  bugprone-*,
  performance-*,
  readability-*,
  modernize-*,
  -modernize-use-trailing-return-type,
  -readability-identifier-length
WarningsAsErrors: '*'
HeaderFilterRegex: '.*'
```

---

## P4: 单元测试与模糊测试

### 4.1 JSON 解析器单元测试

**测试文件**: `server/tests/unit/test_json_parser.c`

测试用例:
```
□ 基本类型: null, true, false, 整数, 浮点数, 字符串
□ 嵌套对象: {"a":{"b":{"c":1}}}
□ 数组: [1,2,3], [[1,2],[3,4]]
□ 转义字符: \n, \t, \\, \", \uXXXX
□ 边界: 空对象 {}, 空数组 [], 空字符串 ""
□ 错误路径: 不完整 JSON, 多重逗号, 未闭合引号, 无效数字
□ 大负载: 深度嵌套 (32+ 层), 长字符串 (64KB+)
□ Unicode: UTF-8 BOM, 多字节字符, 无效 UTF-8 序列
```

### 4.2 JSON 解析器模糊测试

**测试文件**: `tests/fuzz/fuzz_json_parser.c`

```c
// libFuzzer 入口
int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // 以 null 结尾的输入
    char* input = (char*)malloc(size + 1);
    memcpy(input, data, size);
    input[size] = '\0';
    
    JsonValue* val = json_parse(input);
    json_value_free(val);
    free(input);
    return 0;
}
```

构建方式:
```bash
clang -fsanitize=fuzzer -Iinclude tests/fuzz/fuzz_json_parser.c server/src/json_parser.c -o fuzz_json_parser
./fuzz_json_parser -max_len=4096 corpus/
```

### 4.3 协议/消息/数据库单元测试

| 模块 | 测试文件 | 测试内容 |
|------|---------|---------|
| 协议编码 | `tests/unit/test_protocol.c` | 消息序列化/反序列化, 边界长度 |
| HTTP 解析 | `tests/unit/test_http_parser.c` | 请求行解析, 头部解析, 块传输 |
| 数据库 | `tests/unit/test_db_core.c` | CRUD 操作, ID 分配, 文件 I/O |
| WebSocket | `tests/unit/test_websocket.c` | 帧编解码, 握手 |
| 用户处理 | `tests/unit/test_user_handler.c` | 输入验证, 注册/登录逻辑 |

### 4.4 测试基础设施

```
tests/
├── unit/                  # 单元测试 (C, 手写或 CTest)
│   ├── test_json_parser.c
│   ├── test_protocol.c
│   ├── test_http_parser.c
│   ├── test_db_core.c
│   ├── test_websocket.c
│   └── test_user_handler.c
├── fuzz/                  # 模糊测试 (libFuzzer)
│   └── fuzz_json_parser.c
├── api_verification_test.sh
├── security_pen_test.sh
└── loopback_test.sh
```

单元测试框架建议: **minunit.h** (轻量, 单头文件, ~50 行宏) 或 **Unity** (成熟, 主流 C 项目使用)

```c
// minunit.h 示例
#define mu_assert(test, msg) do { if (!(test)) return msg; } while(0)
#define mu_run_test(test) do { char* msg = test(); tests_run++; if (msg) return msg; } while(0)
extern int tests_run;
```

---

## P5: 并发模型与错误处理

### 5.1 当前并发模型分析

```
事件循环主线程 (event_loop)
    ├── accept → 新连接
    ├── WSAPoll/epoll_wait → 就绪连接
    │   ├── conn_read → parse_http_request → find_route → handler
    │   └── conn_write → build_http_response → send
    └── timeout_check → 关闭超时连接
```

**问题**:
- 单线程事件循环，所有 handler 在主循环中同步执行
- 阻塞操作 (文件 I/O, TLS handshake) 会阻塞事件循环
- 无线程池处理长时间运行的 handler

### 5.2 改进方案: 线程池 + 异步任务队列

```
事件循环线程 (event_loop)
    ├── accept → 新连接
    ├── WSAPoll/epoll_wait → 就绪连接
    │   ├── conn_read → 解析请求头 → 入队 task_queue
    │   └── conn_write → 发送响应
    └── timeout_check

线程池 (4-8 线程)
    ├── dequeue task
    ├── 执行 handler (含阻塞操作)
    └── 完成 → enqueue 写任务到 event_loop

结果队列 (写回)
    └── event_loop 处理写完成 → 发送响应
```

**实现文件**: `server/src/http_threadpool.c`, `server/include/http_threadpool.h`

### 5.3 统一错误处理

当前问题: 错误处理分散在各 handler，重复代码多。

方案: 引入统一错误码系统和错误记录层。

```c
// server/include/error.h (新增)
typedef enum {
    ERR_OK = 0,
    ERR_NOMEM,           // 内存不足
    ERR_NOT_FOUND,       // 资源不存在
    ERR_INVALID_PARAM,   // 参数无效
    ERR_PERM_DENIED,     // 权限不足
    ERR_IO_ERROR,        // I/O 错误
    ERR_PARSE_ERROR,     // 解析错误
    ERR_OVERFLOW,        // 缓冲区溢出/截断
    ERR_TIMEOUT,         // 超时
    ERR_INTERNAL,        // 内部错误
} ErrorCode;

// 统一的错误日志 + 返回
#define RETURN_ERR(code, fmt, ...) \
    do { \
        LOG_ERROR("[%s:%d] " fmt, __FILE__, __LINE__, ##__VA_ARGS__); \
        return (code); \
    } while(0)

#define CHECK_NULL(ptr, code, fmt, ...) \
    do { \
        if (!(ptr)) { RETURN_ERR(code, fmt, ##__VA_ARGS__); } \
    } while(0)
```

---

## P6: FFI 与 TLS 审计

### 6.1 rust_stubs.c 审计

当前 FFI 接口:

| 函数 | 谁分配内存 | 谁释放 | 问题 |
|------|-----------|--------|------|
| `rust_generate_jwt()` | Rust (CString) | C (rust_free_string) | ✅ 已有释放函数 |
| `rust_verify_jwt()` | Rust (CString) | C (rust_free_string) | ✅ |
| `rust_hash_password()` | Rust (CString) | C (rust_free_string) | ✅ |
| `rust_verify_password()` | Rust (int) | — | ✅ (返回值) |

**改进**:
```c
// 为每个 FFI 函数添加 C 侧包装，进行返回值和 NULL 检查
char* safe_rust_generate_jwt(const char* user_id) {
    if (!user_id) return NULL;
    char* result = rust_generate_jwt(user_id);
    if (!result) {
        LOG_ERROR("rust_generate_jwt returned NULL for user_id=%s", user_id);
        return NULL;
    }
    return result;  // 调用者需 rust_free_string()
}
```

### 6.2 TLS 审计

**证书文件检查**:
- `server/certs/server.crt` — 确认是自签名测试证书，非生产私钥
- `server/certs/server.key` — 确认无密码保护 (开发环境)

**改进**:
1. 将证书文件添加到 `.gitignore` (排除仓库)
2. 添加 `docs/certs/` 目录存放示例证书模板 (占位文件)
3. 首次启动自动生成证书的逻辑保留

---

## P7: 构建系统与可移植性

### 7.1 CMake 现代化

当前问题: 同时存在 `CMakeLists.txt` 和 `Makefile`，配置分散。

目标: 统一为 CMake，Makefile 作为可选兼容层。

**关键改进**:

```cmake
# 使用 target-based API (现代 CMake)
add_library(chrono_db
    src/db_core.c
    src/db_users.c
    src/db_messages.c
    src/db_friends.c
    src/db_templates.c
)
target_include_directories(chrono_db PUBLIC include)
target_compile_features(chrono_db PUBLIC c_std_99)

# 平台检测抽象到 CMake
if(WIN32)
    target_sources(chrono_http PRIVATE src/platform_win.c)
    target_link_libraries(chrono_http PRIVATE ws2_32)
elseif(UNIX)
    target_sources(chrono_http PRIVATE src/platform_posix.c)
    target_link_libraries(chrono_http PRIVATE pthread m)
endif()
```

### 7.2 合并 CMakeLists.txt

```
server/CMakeLists.txt
├── chrono_core (lib)       # 核心库: json_parser + protocol + utils
├── chrono_db (lib)         # 数据库: db_core + db_users + ...
├── chrono_http (lib)       # HTTP: http_core + http_conn + http_parser + ...
├── chrono_ws (lib)         # WebSocket
├── chrono_tls (lib)        # TLS (条件编译)
├── chrono_security (lib)   # Rust FFI 包装
├── chrono-server (exe)     # 主程序
├── debug_cli (exe)         # 调试工具
└── stress_test (exe)       # 压力测试
```

### 7.3 平台检测优化

从 `platform_compat.h` 中的 `#ifdef` 宏移到 CMake 检测:

```cmake
# CMake 检测 epoll/poll/WSAPoll
include(CheckSymbolExists)
check_symbol_exists(epoll_create sys/epoll.h HAVE_EPOLL)
check_symbol_exists(poll poll.h HAVE_POLL)

if(HAVE_EPOLL)
    target_compile_definitions(chrono_http PUBLIC HAVE_EPOLL=1)
elseif(HAVE_POLL)
    target_compile_definitions(chrono_http PUBLIC HAVE_POLL=1)
else()
    # Windows: WSAPoll
    target_compile_definitions(chrono_http PUBLIC HAVE_WSAPOLL=1)
endif()
```

---

## P8: 代码质量改进

### 8.1 编译警告强化

```cmake
# 统一的编译标志
target_compile_options(chrono_core PRIVATE
    $<$<CONFIG:Debug>:
        -Wall -Wextra -Wpedantic
        -Wstrict-prototypes -Wmissing-prototypes
        -Wshadow -Wpointer-arith -Wcast-qual
        -Wwrite-strings -Wconversion -Wsign-compare
    >
    $<$<CONFIG:Release>:
        -Wall -Wextra
    >
)

# CI 中启用 -Werror
if(CI)
    target_compile_options(chrono_core PRIVATE -Werror)
endif()
```

### 8.2 Doxygen 文档

为所有公共 API 头文件添加 Doxygen 注释:

```c
/**
 * @brief 发送消息并存储到数据库
 * 
 * @param from_id 发送方用户 ID
 * @param to_id 接收方用户 ID  
 * @param content 消息内容 (加密后)
 * @param message_id [out] 新消息 ID
 * @return 0 成功, -1 失败
 */
int db_save_message(int64_t from_id, int64_t to_id, 
                    const char* content, int64_t* message_id);
```

### 8.3 统一日志层

```c
// server/include/logger.h (新增)
typedef enum { LOG_TRACE, LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR } LogLevel;

void log_set_level(LogLevel level);
void log_write(LogLevel level, const char* file, int line, const char* fmt, ...);

#define LOG_TRACE(fmt, ...)  log_write(LOG_TRACE, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...)  log_write(LOG_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)   log_write(LOG_INFO, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)   log_write(LOG_WARN, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...)  log_write(LOG_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
```

当前日志定义在 `server.h` 中，应分离到独立模块。

---

## 项目结构最终形态

```
server/
├── include/
│   ├── server.h              # (保留，精简)
│   ├── http_server.h         # (保留为公共 API)
│   ├── http_conn.h           # [新增] 连接管理
│   ├── http_route.h          # [新增] 路由表
│   ├── http_parser.h         # [新增] 请求解析
│   ├── http_response.h       # [新增] 响应构建
│   ├── http_threadpool.h     # [新增] 线程池
│   ├── db_core.h             # [新增] 数据库核心
│   ├── db_users.h            # [新增] 用户数据
│   ├── db_messages.h         # [新增] 消息数据
│   ├── db_friends.h          # [新增] 好友数据
│   ├── db_templates.h        # [新增] 模板数据
│   ├── database.h            # (保留兼容或删除)
│   ├── platform_compat.h     # (精简为宏+类型+原型)
│   ├── logger.h              # [新增] 统一日志
│   ├── error.h               # [新增] 错误码
│   └── ... (其他现有头文件保留)
│
├── src/
│   ├── main.c                # (不变)
│   ├── http_core.c           # [拆分] 服务器生命周期
│   ├── http_conn.c           # [拆分] 连接管理
│   ├── http_parser.c         # [拆分] HTTP 请求解析
│   ├── http_route.c          # [拆分] 路由表
│   ├── http_response.c       # [拆分] 响应构建
│   ├── http_threadpool.c     # [新增] 线程池
│   ├── db_core.c             # [拆分] 数据库核心
│   ├── db_users.c            # [拆分] 用户数据
│   ├── db_messages.c         # [拆分] 消息数据
│   ├── db_friends.c          # [拆分] 好友数据
│   ├── db_templates.c        # [拆分] 模板数据
│   ├── platform_compat.c     # [拆分] 跨平台公共实现
│   ├── platform_dir.c        # [拆分] 目录迭代
│   ├── platform_thread.c     # [拆分] 线程/互斥体
│   ├── platform_net.c        # [拆分] 网络初始化
│   ├── logger.c              # [新增] 日志实现
│   └── ... (其他现有文件保留: websocket.c, json_parser.c 等)
│
├── tests/
│   └── unit/                 # [新增] 单元测试
│       ├── test_json_parser.c
│       ├── test_http_parser.c
│       ├── test_db_core.c
│       └── ...
│
├── tests/
│   └── fuzz/                 # [新增] 模糊测试
│       └── fuzz_json_parser.c
│
└── .github/
    └── workflows/
        └── ci.yml            # [新增] CI 工作流
```

---

## 实施路线图

### Phase 7.1 — 模块拆分 (预计: 最大工作量)
| 步骤 | 内容 | 依赖 |
|------|------|------|
| 7.1.1 | `platform_compat.h` → `platform_*.c` 拆分 | 无 |
| 7.1.2 | `database.c` → `db_*.c` 拆分 | 7.1.1 (路径函数) |
| 7.1.3 | `http_server.c` → `http_*.c` 拆分 | 7.1.1 (socket 抽象) |
| 7.1.4 | `client/src/network.c` → `net_*.c` 拆分 | 7.1.1 |
| 7.1.5 | 更新 `CMakeLists.txt` 适配新文件结构 | 7.1.1-4 |
| 7.1.6 | 编译验证 (全部零错误) | 7.1.5 |

### Phase 7.2 — 安全审计 + 单元测试
| 步骤 | 内容 | 依赖 |
|------|------|------|
| 7.2.1 | JSON 解析器代码审计 + 修复 | 无 |
| 7.2.2 | WebSocket/HTTP 解析审计 + 修复 | 无 |
| 7.2.3 | FFI/TLS/文件处理审计 + 修复 | 无 |
| 7.2.4 | 添加 `error.h` 统一错误码 | 无 |
| 7.2.5 | 添加 JSON 解析器单元测试 | 无 |
| 7.2.6 | 添加 JSON 解析器模糊测试入口 | 7.2.5 |

### Phase 7.3 — CI/CD + 构建现代化
| 步骤 | 内容 | 依赖 |
|------|------|------|
| 7.3.1 | 创建 `.github/workflows/ci.yml` | 7.1.5 (编译通过) |
| 7.3.2 | 创建 `.clang-tidy` 配置 | 无 |
| 7.3.3 | CMake 现代化 (target-based API) | 7.1.5 |
| 7.3.4 | ASAN/UBSAN 构建配置 | 7.3.3 |

### Phase 7.4 — 代码质量
| 步骤 | 内容 | 依赖 |
|------|------|------|
| 7.4.1 | 添加 `logger.h` 统一日志层 | 无 |
| 7.4.2 | 全局替换 `printf`/`fprintf` → `LOG_*` | 7.4.1 |
| 7.4.3 | Doxygen 注释公共 API | 无 |
| 7.4.4 | 线程池实现 (并发模型改进) | 7.1.3 |

---

## 总结

| 优先级 | 任务 | 影响范围 | 风险 |
|--------|------|---------|------|
| 🔴 P1 | 模块拆分 | 全部 server/client 源文件重构 | 高 (需重新编译验证) |
| 🔴 P2 | 安全审计 | JSON/HTTP/WS/FFI/TLS 文件 | 中 (发现问题需修复) |
| 🔴 P3 | CI/CD | 新增 `.github/` 目录 | 低 (不影响现有代码) |
| 🟡 P4 | 单元测试 + 模糊测试 | 新增 `tests/unit/` `tests/fuzz/` | 低 (不影响现有代码) |
| 🟡 P5 | 并发模型 + 错误处理 | `http_server.c` / 新增文件 | 中 (架构变更) |
| 🟡 P6 | FFI/TLS 审计 | `rust_stubs.c` / `certs/` | 低 |
| 🟡 P7 | 构建系统 | `CMakeLists.txt` / `Makefile` | 中 (构建系统变更) |
| 🟢 P8 | 代码质量 | 全局 | 低 |
