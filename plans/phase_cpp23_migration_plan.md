# Chrono-shift C→C++23 迁移与并发升级计划

> 用户确认范围：**先改造 CLI 测试工具** (`devtools/cli/` + `tools/stress_test.c`)，加入 C++23 并发与抗压强度测试能力

## 一、现状分析

### 1.1 当前编译标准
- **C++ 标准**: C++17（`client/CMakeLists.txt:7`）
- **C 标准**: C99（根 `CMakeLists.txt:4-5`）
- **编译器**: GCC/MinGW（默认），兼容 MSVC

### 1.2 当前并发能力（极度薄弱）
| 特性 | 使用情况 |
|------|---------|
| `std::mutex` + `std::lock_guard` | 仅在 `Logger.cpp:42-80` 的 5 处使用 |
| `std::thread` | 仅在 `ClientHttpServer.cpp:147` 的 1 处使用（HTTP server loop） |
| `std::atomic` | **未使用** |
| `std::async`/`std::future` | **未使用** |
| `std::jthread`/协程 | **未使用** |

### 1.3 本次改造范围

#### 范围 A：devtools/cli/ — CLI 调试工具 C→C++23（25+ 文件，~5000 行）
```
client/devtools/cli/main.c                    (371 行) - REPL 主入口
client/devtools/cli/net_http.c                HTTP 请求工具函数
client/devtools/cli/devtools_cli.h            (115 行) - 公共头文件
client/devtools/cli/commands/init_commands.c  (96 行) - 命令注册
client/devtools/cli/commands/cmd_health.c
client/devtools/cli/commands/cmd_endpoint.c
client/devtools/cli/commands/cmd_token.c
client/devtools/cli/commands/cmd_user.c
client/devtools/cli/commands/cmd_ipc.c
client/devtools/cli/commands/cmd_ws.c
client/devtools/cli/commands/cmd_msg.c
client/devtools/cli/commands/cmd_friend.c
client/devtools/cli/commands/cmd_db.c
client/devtools/cli/commands/cmd_session.c
client/devtools/cli/commands/cmd_config.c
client/devtools/cli/commands/cmd_storage.c
client/devtools/cli/commands/cmd_ping.c
client/devtools/cli/commands/cmd_watch.c
client/devtools/cli/commands/cmd_rate_test.c
client/devtools/cli/commands/cmd_json.c
client/devtools/cli/commands/cmd_tls.c
client/devtools/cli/commands/cmd_trace.c
client/devtools/cli/commands/cmd_connect.c
client/devtools/cli/commands/cmd_disconnect.c
client/devtools/cli/commands/cmd_crypto.c
client/devtools/cli/commands/cmd_network.c
client/devtools/cli/commands/cmd_gen_cert.c
client/devtools/cli/commands/cmd_obfuscate.c
```

#### 范围 B：tools/stress_test.c — 压力测试工具 C→C++23 并发升级（776 行）
```
client/tools/stress_test.c  (776 行) - 当前纯 C 的压力测试
```

#### 本次不包含（后续阶段处理）
- `client/tools/debug_cli.c`（3093 行老旧单体，已在被 devtools/cli/ 替代中）
- `client/src/network/tls_client.c`（已有 C++ TlsWrapper.cpp 包装）

## 二、CLI 工具 C→C++23 架构设计

### 2.1 核心类型系统重构

```cpp
// devtools_cli.h → 命名空间 + 类设计

namespace chrono::client::cli {

// C++23: std::flat_map 替代 CommandEntry[] 数组
// C++23: std::move_only_function 替代函数指针
// C++23: std::string_view 替代 const char*
// C++23: std::expected 替代 int 返回值错误码
// C++23: std::println 替代 printf

class Command {
public:
    std::string_view name;
    std::string_view description;
    std::string_view usage;
    std::move_only_function<int(std::span<char*>)> handler;
    // C++20 <=> 比较运算符
    auto operator<=>(const Command&) const = default;
};

class CommandRegistry {
    std::flat_map<std::string, Command> commands_;
public:
    void register_command(Command cmd);
    std::optional<std::reference_wrapper<const Command>> find(std::string_view name) const;
    void execute(std::string_view name, std::span<char*> args) const;
    void list_all() const;  // 用 std::println 格式化输出
};

struct Config {
    std::string host = "127.0.0.1";
    int port = 8443;
    bool use_tls = true;
    bool session_logged_in = false;
    std::string session_token;
    std::string storage_path;
    bool verbose = false;
    
    // RAII 构造
    Config();
    ~Config();
};

// TLS RAII 包装 — 替代 void* ws_ssl
// C++23: std::inout_ptr 简化 OpenSSL C 指针管理
class TlsRaii {
    std::unique_ptr<SSL_CTX, decltype(&SSL_CTX_free)> ctx_;
    std::unique_ptr<SSL, decltype(&SSL_free)> ssl_;
public:
    TlsRaii();
    int connect(int fd, const char* host);
    int read(void* buf, int num);
    int write(const void* buf, int num);
};

// HTTP 请求 — 替代 net_http.c 的 C 风格函数
// C++23: 协程异步版本
struct HttpResponse {
    int status_code;
    std::string body;
    std::flat_map<std::string, std::string> headers;
};

class HttpClient {
public:
    std::expected<HttpResponse, std::string> request(
        std::string_view method,
        std::string_view path,
        std::string_view body = {},
        std::span<const std::pair<std::string_view, std::string_view>> headers = {});
};

} // namespace
```

### 2.2 文件映射

| C 文件 | C++23 目标 | 关键改动 |
|--------|-----------|---------|
| `devtools_cli.h` | `devtools_cli.hpp` | 命名空间封装、`std::flat_map`、`std::move_only_function`、`std::string_view` |
| `main.c` | `main.cpp` | REPL 使用 `std::println`，命令解析 `std::span`，错误处理 `std::expected` |
| `net_http.c` | `net_http.cpp` | `HttpClient` 类 + 协程异步版本 |
| `init_commands.c` | `init_commands.cpp` | 自动注册 via `CommandRegistry::register_command()` |
| `cmd_*.c` | `cmd_*.cpp` | 函数 → 命名空间自由函数 + `std::expected` 返回值 |

## 三、抗压强度测试并发升级（stress_test）

### 3.1 当前 stress_test.c 架构（纯 C）
- 手动 `CreateThread`/`pthread_create`
- 原始 socket API
- 全局变量管理状态
- 无同步原语

### 3.2 C++23 并发升级方案

```cpp
// stress_test.cpp — C++23 并发压力测试
namespace chrono::client::tools {

class StressTest {
    struct Config {
        std::string host;
        int port;
        int concurrency = 10;     // 并发数
        int total_requests = 100;  // 总请求数
        std::chrono::seconds duration{30};
        bool use_tls = true;
    };

    struct TestResult {
        int success_count{0};
        int failure_count{0};
        std::chrono::nanoseconds total_latency{0};
        std::vector<std::chrono::nanoseconds> latencies;
        int min_latency_ms{INT_MAX};
        int max_latency_ms{0};
    };

    Config config_;
    std::atomic<int> active_requests_{0};     // 当前活跃请求数
    std::atomic<int> completed_requests_{0};  // 已完成请求数
    std::atomic<int> failed_requests_{0};     // 失败请求数
    TestResult result_;
    
    // C++20: 并发控制
    std::counting_semaphore<> concurrency_limit_;
    
    // C++20: 阶段性同步
    std::barrier<> phase_barrier_;
    
    // C++23: 移 Only 函数回调
    std::move_only_function<void(const TestResult&)> on_complete_;
    
public:
    // C++23: std::expected 返回结果
    std::expected<TestResult, std::string> run(const Config& cfg);
    
    // C++20: jthread 自动管理生命周期
    void start_worker(int worker_id, std::stop_token stop);
    
    // 实时统计输出
    void report_progress();
};

} // namespace
```

### 3.3 并发特性应用映射

| C++23/20 特性 | StressTest 应用 |
|---------------|----------------|
| `std::jthread` + `std::stop_token` | 每个 worker 一个自动管理线程，`stop_token` 支持优雅停止 |
| `std::latch` | 等待所有 worker 就绪后同时开始施压 |
| `std::barrier` | 多阶段压力测试（例如 100→500→1000 并发阶梯增压） |
| `std::counting_semaphore` | 控制最大并发数，超出发起排队等待 |
| `std::atomic` | 无锁计数器：活跃请求、成功/失败计数 |
| `std::expected` | 统一错误传播，替代错误码 + 异常混合 |
| `std::println` | 格式化实时进度、统计报告 |
| `std::chrono` | 高精度延迟测量、统计（P50/P90/P99 延迟） |

## 四、分步实施方案

### 步骤 1：构建系统准备
- 更新 `client/CMakeLists.txt`: `CMAKE_CXX_STANDARD 17` → `23`
- 编译器版本检测（GCC 13+ / MSVC 2022 17.6+）
- 将 `devtools/cli/` 和 `tools/` 纳入 CMake 构建体系

### 步骤 2：CLI 核心基础设施 C++23 化
- `devtools_cli.h` → `devtools_cli.hpp`：命名空间 + `CommandRegistry` + `Config` 类
- `main.c` → `main.cpp`：`std::println` REPL + `std::expected` 错误传播
- `net_http.c` → `net_http.cpp`：`HttpClient` RAII 类 + TLS RAII 包装

### 步骤 3：CLI 命令模块 C++23 化（逐个转换 28 个 cmd_*.c）
- 每个 cmd_*.c → cmd_*.cpp
- 函数签名从 `int cmd_xxx(int argc, char** argv)` 改为命名空间级 lambda 注册
- `init_commands.c` → 自动注册模式

### 步骤 4：压力测试工具并发升级
- `stress_test.c` → `stress_test.cpp`
- 实现 `StressTest` 类 + `std::jthread` worker 池 + `std::barrier` 分阶增压
- 加入 P50/P90/P99 延迟统计
- 加入 `std::counting_semaphore` 并发控制

### 步骤 5：Makefile → CMake 迁移
- 移除 `client/devtools/cli/Makefile`（独立 Makefile）
- 移除 `client/tools/Makefile`
- 全部通过 CMake 统一构建

## 五、构建系统变更

### CMakeLists.txt 变更

```cmake
# client/CMakeLists.txt
cmake_minimum_required(VERSION 3.20)  # C++23 需要较新 CMake
project(chrono-client LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 编译器版本检查
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 13)
    message(FATAL_ERROR "C++23 需要 GCC 13+，当前: ${CMAKE_CXX_COMPILER_VERSION}")
endif()
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 1936)
    message(FATAL_ERROR "C++23 需要 MSVC 2022 17.6+，当前: ${CMAKE_CXX_COMPILER_VERSION}")
endif()

# 源文件 — 移除 C 源文件
file(GLOB_RECURSE CLIENT_SOURCES
    src/util/*.cpp
    src/network/*.cpp
    src/app/*.cpp
    src/storage/*.cpp
    src/security/*.cpp
    src/ai/*.cpp
    src/plugin/*.cpp
    devtools/core/*.cpp
    devtools/cli/*.cpp
    devtools/cli/commands/*.cpp
    tools/*.cpp
)
```

## 六、预期成果

1. **CLI 工具完全 C++23**：类型安全、RAII 资源管理、无手动内存释放
2. **并发压力测试**：支持可配置并发数、阶梯增压、P50/P90/P99 延迟统计
3. **统一构建**：所有组件通过 CMake 一键构建
4. **未来可扩展**：命令注册表可动态扩展，StressTest 框架可复用

## 七、技术风险

| 风险 | 缓解措施 |
|------|---------|
| GCC 版本不足 13 | CMake 编译时检测，提供降级到 C++20 选项 |
| `std::flat_map` 在 C++23 正式标准化但实现滞后 | 回退到 `std::map` + `absl::flat_hash_map` 作为 fallback |
| Windows MinGW 对 C++23 协程支持 | 优先用 `std::jthread` + `std::barrier`，协程作为增强选项 |
| CLI 的 socket 操作与 `std::jthread` 取消交互 | 使用 `stop_token` + `select()`/`WSAPoll()` 实现可中断 I/O |
