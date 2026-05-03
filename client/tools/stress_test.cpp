/**
 * 墨竹 (Chrono-shift) 压力测试框架 (C++23)
 *
 * 用途：评估服务器在高负载下的抗冲击能力
 * 特性：
 *   - C++23 jthread + atomic 并发
 *   - 可配置 QPS 目标、线程数、持续时间
 *   - 实时统计：QPS、延迟(P50/P95/P99)、错误率
 *   - 支持多种测试场景
 *   - std::chrono 跨平台高精度计时
 *   - HTTPS (TLS/SSL) 支持 (强制启用)
 *
 * 编译:
 *   Windows: g++ -std=c++23 -Wall -I../include stress_test.cpp -o stress_test
 *              -lws2_32 -LD:/mys32/mingw64/lib -lssl -lcrypto
 *   Linux:   g++ -std=c++23 -Wall -I../include stress_test.cpp -o stress_test
 *              -lpthread -lssl -lcrypto
 *
 * 使用:
 *   stress_test --host 127.0.0.1 --port 4443 --threads 4 --qps 100 --duration 30
 */
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <latch>
#include <print>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#ifdef _WIN32
    #include <winsock2.h>
    #include <windows.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <arpa/inet.h>
    #include <cerrno>
    #include <netdb.h>
    #include <netinet/in.h>
    #include <sys/socket.h>
    #include <unistd.h>
#endif

// ============================================================
// TLS C 函数 extern (类型擦除: void* 代替 SSL*)
// tls_client.c 编译为 C, 需要 extern "C" 链接
// ============================================================
extern "C" {
extern int tls_client_init(const char* cert_dir);
extern int tls_client_connect(void** ssl, const char* host, unsigned short port);
extern int tls_write(void* ssl, const char* data, size_t len);
extern int tls_read(void* ssl, char* buf, size_t len);
extern void tls_close(void* ssl);
}

// ============================================================
// 别名
// ============================================================
using namespace std::chrono_literals;

// ============================================================
// 常量
// ============================================================
constexpr size_t MAX_URL_LEN = 512;
constexpr size_t MAX_HEADERS = 64;
constexpr size_t MAX_HEADER_LEN = 256;
constexpr size_t MAX_RESP_BUF = 65536;
constexpr int MAX_THREADS = 64;
constexpr int BENCHMARK_RUNS = 5;

// ============================================================
// 测试场景
// ============================================================
struct TestScenario {
    const char* name;
    const char* method;
    const char* path;
    const char* body;
    const char* content_type;
};

static constexpr TestScenario SCENARIOS[] = {
    {"健康检查",   "GET",    "/api/health",          nullptr, nullptr},
    {"用户注册",   "POST",   "/api/user/register",
     "{\"username\":\"stress_test_user\",\"password\":\"Test123456\","
     "\"nickname\":\"压力测试\"}", "application/json"},
    {"用户登录",   "POST",   "/api/user/login",
     "{\"username\":\"stress_test_user\",\"password\":\"Test123456\"}",
     "application/json"},
    {"获取模板",   "GET",    "/api/templates",       nullptr, nullptr},
    {"发送消息",   "POST",   "/api/message/send",
     "{\"to_user_id\":\"test\",\"content\":\"压力测试消息\"}",
     "application/json"},
    {nullptr, nullptr, nullptr, nullptr, nullptr}  /* 结束标记 */
};

// ============================================================
// 统计 (原子类型)
// ============================================================
struct Stats {
    std::atomic<long long> total_requests{0};
    std::atomic<long long> success_count{0};
    std::atomic<long long> error_count{0};
    std::atomic<long long> total_latency_us{0};
    std::atomic<long long> min_latency_us{0};
    std::atomic<long long> max_latency_us{0};
    double* latencies = nullptr;           // 延迟样本数组
    std::atomic<int> latency_count{0};
    int latency_capacity = 0;
    std::atomic<bool> running{false};
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point end_time;
};

// ============================================================
// 命令行参数
// ============================================================
struct Config {
    std::string host = "127.0.0.1";
    int port = 4443;
    int threads = 4;
    int qps_target = 100;
    int duration_sec = 30;
    int scenario_index = 0;
    bool use_ssl = true;       // HTTPS 强制启用
    bool verbose = false;
};

// ============================================================
// 全局变量
// ============================================================
static Config g_config;
static Stats g_stats;
static std::atomic<bool> g_running{true};

// ============================================================
// 工具函数
// ============================================================

[[nodiscard]] static long long get_time_us() noexcept {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(
        now.time_since_epoch()).count();
}

[[nodiscard]] static long long elapsed_ms(
    std::chrono::steady_clock::time_point start) noexcept {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        now - start).count();
}

#ifdef _WIN32
static bool init_winsock() noexcept {
    WSADATA wsa;
    return WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
}

static void cleanup_winsock() noexcept {
    WSACleanup();
}
#endif

/* 安全 float -> int 转换 */
[[nodiscard]] static int safe_round(double val) noexcept {
    if (val < 0) return 0;
    if (val > 1e9) return static_cast<int>(1e9);
    return static_cast<int>(val + 0.5);
}

// ============================================================
// HTTP 请求
// ============================================================

static int http_request(
    const char* method, const char* path,
    const char* body, const char* content_type,
    char* response, int resp_size,
    long long* latency_us
) noexcept {
    char request[8192];
    int ret = -1;
    long long t0, t1;

    /* 构建 HTTP 请求 */
    if (body && content_type) {
        std::snprintf(request, sizeof(request),
            "%s %s HTTP/1.1\r\n"
            "Host: %s:%d\r\n"
            "Content-Type: %s\r\n"
            "Content-Length: %zu\r\n"
            "Connection: close\r\n"
            "\r\n"
            "%s",
            method, path,
            g_config.host.c_str(), g_config.port,
            content_type,
            std::strlen(body),
            body
        );
    } else {
        std::snprintf(request, sizeof(request),
            "%s %s HTTP/1.1\r\n"
            "Host: %s:%d\r\n"
            "Connection: close\r\n"
            "\r\n",
            method, path,
            g_config.host.c_str(), g_config.port
        );
    }

    if (g_config.use_ssl) {
        // ================================================================
        // HTTPS 模式: 使用 tls_client API 建立加密连接
        // ================================================================
        void* ssl = nullptr;

        /* 计时开始 (包含 TCP 连接 + TLS 握手 + 请求收发) */
        t0 = get_time_us();

        /* tls_client_init 在 worker 线程中首次调用时初始化全局 ctx */
        if (tls_client_init(nullptr) != 0) {
            return -1;
        }

        /* 连接 (TCP + TLS 握手) */
        if (tls_client_connect(&ssl, g_config.host.c_str(),
                static_cast<uint16_t>(g_config.port)) != 0) {
            return -1;
        }

        /* 发送请求 */
        {
            int total = 0;
            int len = static_cast<int>(std::strlen(request));
            while (total < len) {
                int n = static_cast<int>(tls_write(ssl, request + total,
                    static_cast<size_t>(len - total)));
                if (n <= 0) { tls_close(ssl); return -1; }
                total += n;
            }
        }

        /* 接收响应 */
        {
            int total = 0;
            int n;
            while (total < resp_size - 1) {
                n = static_cast<int>(tls_read(ssl, response + total,
                    static_cast<size_t>(resp_size - 1 - total)));
                if (n <= 0) break;
                total += n;
            }
            response[total] = '\0';
        }

        /* 计时结束 */
        t1 = get_time_us();

        tls_close(ssl);
    } else {
        // ================================================================
        // HTTP 明文模式 (用于开发/调试)
        // ================================================================
#ifdef _WIN32
        SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock == INVALID_SOCKET) return -1;
#else
        int sock = static_cast<int>(::socket(AF_INET, SOCK_STREAM, 0));
        if (sock < 0) return -1;
#endif

        struct sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));

        /* DNS 解析 */
        struct hostent* host = gethostbyname(g_config.host.c_str());
        if (!host) {
#ifdef _WIN32
            closesocket(sock);
#else
            ::close(sock);
#endif
            return -1;
        }

        /* 连接 */
        addr.sin_family = AF_INET;
        addr.sin_port = htons(static_cast<unsigned short>(g_config.port));
        std::memcpy(&addr.sin_addr, host->h_addr_list[0],
                    static_cast<size_t>(host->h_length));

#ifdef _WIN32
        if (connect(sock, reinterpret_cast<struct sockaddr*>(&addr),
                sizeof(addr)) == SOCKET_ERROR) {
            closesocket(sock);
            return -1;
        }
#else
        if (::connect(sock, reinterpret_cast<struct sockaddr*>(&addr),
                sizeof(addr)) < 0) {
            ::close(sock);
            return -1;
        }
#endif

        /* 计时开始 */
        t0 = get_time_us();

        /* 发送请求 */
#ifdef _WIN32
        if (send(sock, request, static_cast<int>(std::strlen(request)), 0)
                == SOCKET_ERROR) {
            closesocket(sock);
            return -1;
        }
#else
        if (::send(sock, request, static_cast<int>(std::strlen(request)), 0)
                < 0) {
            ::close(sock);
            return -1;
        }
#endif

        /* 接收响应 */
        {
            int total = 0;
            int n;
            while (total < resp_size - 1) {
#ifdef _WIN32
                n = recv(sock, response + total,
                    static_cast<int>(resp_size - 1 - total), 0);
#else
                n = static_cast<int>(::read(sock, response + total,
                    static_cast<size_t>(resp_size - 1 - total)));
#endif
                if (n <= 0) break;
                total += n;
            }
            response[total] = '\0';
        }

        /* 计时结束 */
        t1 = get_time_us();

#ifdef _WIN32
        closesocket(sock);
#else
        ::close(sock);
#endif
    }

    if (latency_us) *latency_us = t1 - t0;

    /* 检查 HTTP 状态码 */
    if (std::strncmp(response, "HTTP/1.1 200", 12) == 0 ||
        std::strncmp(response, "HTTP/1.0 200", 12) == 0) {
        ret = 0;  /* 成功 */
    } else if (std::strstr(response, "HTTP/1.1") ||
               std::strstr(response, "HTTP/1.0")) {
        ret = 1;  /* 非 200 但有效响应 */
    } else {
        ret = -1; /* 无效响应 */
    }

    return ret;
}

// ============================================================
// 工作线程
// ============================================================

static void worker_thread(std::stop_token stoken) {
    char resp[MAX_RESP_BUF];
    const TestScenario* scenario = &SCENARIOS[g_config.scenario_index];
    long long interval_us = (g_config.qps_target > 0)
        ? (1000000LL / g_config.qps_target) : 0;
    long long next_wake = get_time_us();

    while (!stoken.stop_requested()) {
        long long latency = 0;
        int result;

        /* 执行请求 */
        result = http_request(
            scenario->method,
            scenario->path,
            scenario->body,
            scenario->content_type,
            resp, sizeof(resp),
            &latency
        );

        /* 更新统计 (原子操作) */
        if (result == 0) {
            g_stats.success_count.fetch_add(1, std::memory_order_relaxed);
        } else {
            g_stats.error_count.fetch_add(1, std::memory_order_relaxed);
        }
        g_stats.total_requests.fetch_add(1, std::memory_order_relaxed);
        g_stats.total_latency_us.fetch_add(latency, std::memory_order_relaxed);

        /* 记录延迟样本 */
        int idx = g_stats.latency_count.fetch_add(1, std::memory_order_relaxed);
        if (idx < g_stats.latency_capacity) {
            g_stats.latencies[idx] = static_cast<double>(latency);
        }

        /* 更新最大/最小延迟 */
        long long min_val = g_stats.min_latency_us.load(std::memory_order_relaxed);
        while ((min_val == 0 || latency < min_val) &&
               !g_stats.min_latency_us.compare_exchange_weak(
                   min_val, latency, std::memory_order_relaxed)) {}

        long long max_val = g_stats.max_latency_us.load(std::memory_order_relaxed);
        while (latency > max_val &&
               !g_stats.max_latency_us.compare_exchange_weak(
                   max_val, latency, std::memory_order_relaxed)) {}

        /* QPS 控制 */
        if (interval_us > 0) {
            next_wake += interval_us;
            long long now = get_time_us();
            if (next_wake > now && (next_wake - now) < 1000000) {
                std::this_thread::sleep_for(
                    std::chrono::microseconds(next_wake - now));
            } else if (next_wake <= now) {
                next_wake = now;
            }
        }
    }
}

// ============================================================
// 比较函数 (用于 qsort)
// ============================================================

static int cmp_double(const void* a, const void* b) noexcept {
    double da = *static_cast<const double*>(a);
    double db = *static_cast<const double*>(b);
    if (da < db) return -1;
    if (da > db) return 1;
    return 0;
}

// ============================================================
// 打印帮助
// ============================================================

static void print_usage() noexcept {
    std::println("墨竹 (Chrono-shift) 压力测试框架");
    std::println("用法: stress_test [选项]\n");
    std::println("选项:");
    std::println("  --host <ip>       服务器地址 (默认: 127.0.0.1)");
    std::println("  --port <port>     服务器端口 (默认: 4443)");
    std::println("  --threads <n>     并发线程数 (默认: 4, 最大: {})", MAX_THREADS);
    std::println("  --qps <n>         QPS 目标 (默认: 100)");
    std::println("  --duration <s>    测试持续时间 (秒) (默认: 30)");
    std::println("  --scenario <n>    测试场景索引 (默认: 0)");
    std::println("  --no-ssl          禁用 HTTPS (使用明文 HTTP, 仅用于调试)");
    std::println("  --list-scenarios  列出所有测试场景");
    std::println("  --verbose         详细输出");
    std::println("  --help            显示此帮助\n");
    std::println("默认启用 HTTPS (TLS) — 服务器仅支持加密连接\n");
    std::println("测试场景:");
    for (int i = 0; SCENARIOS[i].name; i++) {
        std::println("  [{}] {} ({} {})", i, SCENARIOS[i].name,
                     SCENARIOS[i].method, SCENARIOS[i].path);
    }
}

static void print_scenarios() noexcept {
    std::println("可用的测试场景:");
    for (int i = 0; SCENARIOS[i].name; i++) {
        std::println("  [{}] {}", i, SCENARIOS[i].name);
        std::println("      方法: {}", SCENARIOS[i].method);
        std::println("      路径: {}", SCENARIOS[i].path);
        std::println("      请求体: {}",
                     SCENARIOS[i].body ? SCENARIOS[i].body : "(无)");
        std::println("");
    }
}

// ============================================================
// 统计汇总
// ============================================================

static void print_report() noexcept {
    long long elapsed_ms_val = std::chrono::duration_cast<std::chrono::milliseconds>(
        g_stats.end_time - g_stats.start_time).count();
    double elapsed_sec = elapsed_ms_val / 1000.0;
    long long total = g_stats.total_requests.load();
    double actual_qps = (elapsed_sec > 0) ? (total / elapsed_sec) : 0;
    long long total_lat = g_stats.total_latency_us.load();
    double avg_latency_us = (total > 0) ? (static_cast<double>(total_lat) / total) : 0;
    long long success = g_stats.success_count.load();
    long long errors = g_stats.error_count.load();
    long long min_lat = g_stats.min_latency_us.load();
    long long max_lat = g_stats.max_latency_us.load();
    int lat_count = g_stats.latency_count.load();

    std::println("");
    std::println("========================================");
    std::println("  墨竹 压力测试报告");
    std::println("========================================\n");

    /* 测试配置 */
    std::println("【测试配置】");
    std::println("  服务器:      {}:{}", g_config.host, g_config.port);
    std::println("  场景:        {} ({} {})",
        SCENARIOS[g_config.scenario_index].name,
        SCENARIOS[g_config.scenario_index].method,
        SCENARIOS[g_config.scenario_index].path);
    std::println("  并发线程:    {}", g_config.threads);
    std::println("  QPS 目标:    {} req/s", g_config.qps_target);
    std::println("  计划时长:    {} 秒", g_config.duration_sec);
    std::println("  实际时长:    {:.2f} 秒\n", elapsed_sec);

    /* 吞吐量 */
    std::println("【吞吐量】");
    std::println("  总请求数:    {}", total);
    std::println("  成功请求:    {}", success);
    std::println("  失败请求:    {}", errors);
    std::println("  成功率:      {:.2f}%",
        (total > 0) ? (success * 100.0 / total) : 0);
    std::println("  实际 QPS:    {:.2f} req/s", actual_qps);
    std::println("  目标达成率:  {:.2f}%\n",
        (g_config.qps_target > 0) ? (actual_qps * 100.0 / g_config.qps_target) : 0);

    /* 延迟 */
    std::println("【延迟】");
    std::println("  平均延迟:    {:.2f} ms", avg_latency_us / 1000.0);
    std::println("  最小延迟:    {:.2f} ms", static_cast<double>(min_lat) / 1000.0);
    std::println("  最大延迟:    {:.2f} ms", static_cast<double>(max_lat) / 1000.0);

    /* 百分位延迟 */
    if (lat_count > 0) {
        std::qsort(g_stats.latencies, static_cast<size_t>(lat_count),
                   sizeof(double), cmp_double);
        std::println("  P50 延迟:    {:.2f} ms",
            g_stats.latencies[lat_count * 50 / 100] / 1000.0);
        std::println("  P90 延迟:    {:.2f} ms",
            g_stats.latencies[lat_count * 90 / 100] / 1000.0);
        std::println("  P95 延迟:    {:.2f} ms",
            g_stats.latencies[lat_count * 95 / 100] / 1000.0);
        std::println("  P99 延迟:    {:.2f} ms",
            g_stats.latencies[lat_count * 99 / 100] / 1000.0);
    }

    /* 抗冲击评估 */
    std::println("\n【抗冲击能力评估】");
    double error_rate = (total > 0) ? (errors * 100.0 / total) : 0;
    if (error_rate > 20.0) {
        std::println("  等级: ❌ 不合格 (错误率 {:.2f}% > 20%)", error_rate);
        std::println("  建议: 检查服务器资源限制或优化请求处理逻辑");
    } else if (error_rate > 5.0) {
        std::println("  等级: ⚠️ 一般 (错误率 {:.2f}% > 5%)", error_rate);
        std::println("  建议: 适当增加服务器资源或进行代码级优化");
    } else if (error_rate > 1.0) {
        std::println("  等级: ✅ 良好 (错误率 {:.2f}%)", error_rate);
        std::println("  建议: 少量优化即可达到优秀");
    } else {
        std::println("  等级: 🏆 优秀 (错误率 {:.2f}%)", error_rate);
        std::println("  说明: 框架在高负载下表现稳定");
    }

    double target_ratio = (g_config.qps_target > 0)
        ? (actual_qps / g_config.qps_target) : 1;
    if (target_ratio < 0.5) {
        std::println("  QPS 达成:    ❌ 未达标 (仅达成目标的 {:.1f}%)",
                     target_ratio * 100.0);
    } else if (target_ratio < 0.8) {
        std::println("  QPS 达成:    ⚠️ 部分达标 (达成目标的 {:.1f}%)",
                     target_ratio * 100.0);
    } else if (target_ratio < 1.0) {
        std::println("  QPS 达成:    ✅ 接近目标 (达成目标的 {:.1f}%)",
                     target_ratio * 100.0);
    } else {
        std::println("  QPS 达成:    🏆 超越目标 (达成目标的 {:.1f}%)",
                     target_ratio * 100.0);
    }

    std::println("\n========================================\n");

    /* 保存报告到文件 */
    FILE* fp = std::fopen("reports/stress_test_report.md", "w");
    if (fp) {
        std::fprintf(fp, "# 墨竹 压力测试报告\n\n");
        std::fprintf(fp, "## 测试配置\n\n");
        std::fprintf(fp, "| 参数 | 值 |\n");
        std::fprintf(fp, "|------|-----|\n");
        std::fprintf(fp, "| 服务器 | %s:%d |\n",
                     g_config.host.c_str(), g_config.port);
        std::fprintf(fp, "| 场景 | %s |\n",
                     SCENARIOS[g_config.scenario_index].name);
        std::fprintf(fp, "| 并发线程 | %d |\n", g_config.threads);
        std::fprintf(fp, "| QPS 目标 | %d req/s |\n", g_config.qps_target);
        std::fprintf(fp, "| 测试时长 | %.2f 秒 |\n", elapsed_sec);
        std::fprintf(fp, "\n");
        std::fprintf(fp, "## 测试结果\n\n");
        std::fprintf(fp, "| 指标 | 值 |\n");
        std::fprintf(fp, "|------|-----|\n");
        std::fprintf(fp, "| 总请求数 | %lld |\n", total);
        std::fprintf(fp, "| 成功率 | %.2f%% |\n",
            (total > 0) ? (success * 100.0 / total) : 0);
        std::fprintf(fp, "| 实际 QPS | %.2f req/s |\n", actual_qps);
        std::fprintf(fp, "| 平均延迟 | %.2f ms |\n", avg_latency_us / 1000.0);
        std::fprintf(fp, "| P50 延迟 | %.2f ms |\n",
            lat_count > 0 ? g_stats.latencies[lat_count * 50 / 100] / 1000.0 : 0);
        std::fprintf(fp, "| P99 延迟 | %.2f ms |\n",
            lat_count > 0 ? g_stats.latencies[lat_count * 99 / 100] / 1000.0 : 0);
        std::fprintf(fp, "| 错误率 | %.2f%% |\n", error_rate);
        std::fprintf(fp, "| 抗冲击等级 | %s |\n",
            error_rate > 20.0 ? "❌ 不合格" :
            error_rate > 5.0  ? "⚠️ 一般" :
            error_rate > 1.0  ? "✅ 良好" : "🏆 优秀");
        std::fprintf(fp, "\n");
        std::fclose(fp);
        std::println("  报告已保存至: reports/stress_test_report.md");
    }
}

// ============================================================
// 进度显示线程
// ============================================================

static void progress_thread(std::stop_token stoken) {
    int last_total = 0;

    std::println("\n正在执行压力测试...");
    std::println("  目标: {} QPS, {} 线程, {} 秒\n",
                 g_config.qps_target, g_config.threads, g_config.duration_sec);

    while (!stoken.stop_requested()) {
        long long elapsed_ms_val = std::chrono::duration_cast<
            std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - g_stats.start_time).count();
        int seconds = static_cast<int>(elapsed_ms_val / 1000);
        int current_total = static_cast<int>(
            g_stats.total_requests.load(std::memory_order_relaxed));
        int qps_now = current_total - last_total;
        last_total = current_total;

        std::print("\r  [{:02d}:{:02d}] 请求: {} | QPS: {} | 成功: {} | 失败: {}",
            seconds / 60, seconds % 60,
            current_total, qps_now,
            g_stats.success_count.load(std::memory_order_relaxed),
            g_stats.error_count.load(std::memory_order_relaxed));
        std::fflush(stdout);

        std::this_thread::sleep_for(1s);
    }
    std::println("");
}

// ============================================================
// 主函数
// ============================================================

int main(int argc, char* argv[]) {
    std::vector<std::jthread> workers;

#ifdef _WIN32
    if (!init_winsock()) {
        std::println(stderr, "错误: WinSock 初始化失败");
        return 1;
    }
#endif

    /* 解析命令行参数 */
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--host") == 0 && i + 1 < argc) {
            g_config.host = argv[++i];
        } else if (std::strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            g_config.port = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--threads") == 0 && i + 1 < argc) {
            g_config.threads = std::atoi(argv[++i]);
            if (g_config.threads < 1) g_config.threads = 1;
            if (g_config.threads > MAX_THREADS) g_config.threads = MAX_THREADS;
        } else if (std::strcmp(argv[i], "--qps") == 0 && i + 1 < argc) {
            g_config.qps_target = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--duration") == 0 && i + 1 < argc) {
            g_config.duration_sec = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--scenario") == 0 && i + 1 < argc) {
            g_config.scenario_index = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--no-ssl") == 0) {
            g_config.use_ssl = false;
        } else if (std::strcmp(argv[i], "--list-scenarios") == 0) {
            print_scenarios();
#ifdef _WIN32
            cleanup_winsock();
#endif
            return 0;
        } else if (std::strcmp(argv[i], "--verbose") == 0) {
            g_config.verbose = true;
        } else if (std::strcmp(argv[i], "--help") == 0) {
            print_usage();
#ifdef _WIN32
            cleanup_winsock();
#endif
            return 0;
        }
    }

    /* 验证场景索引 */
    int scenario_count = 0;
    while (SCENARIOS[scenario_count].name) scenario_count++;
    if (g_config.scenario_index < 0 ||
        g_config.scenario_index >= scenario_count) {
        std::println(stderr, "错误: 无效的场景索引 {} (可用: 0-{})",
                     g_config.scenario_index, scenario_count - 1);
#ifdef _WIN32
        cleanup_winsock();
#endif
        return 1;
    }

    std::println("========================================");
    std::println("  墨竹 (Chrono-shift) 压力测试");
    std::println("========================================");
    std::println("  服务器:  {}:{}", g_config.host, g_config.port);
    std::println("  协议:    {}", g_config.use_ssl ? "HTTPS (TLS)" : "HTTP (明文)");
    std::println("  场景:    {}", SCENARIOS[g_config.scenario_index].name);
    std::println("  线程数:  {}", g_config.threads);
    std::println("  QPS:     {}", g_config.qps_target);
    std::println("  时长:    {} 秒", g_config.duration_sec);
    std::println("========================================\n");

    /* 初始化统计区 */
    g_stats.latency_capacity = g_config.qps_target * g_config.duration_sec * 2;
    if (g_stats.latency_capacity < 10000)
        g_stats.latency_capacity = 10000;
    g_stats.latencies = static_cast<double*>(
        std::calloc(static_cast<size_t>(g_stats.latency_capacity),
                    sizeof(double)));
    if (!g_stats.latencies) {
        std::println(stderr, "错误: 内存不足");
#ifdef _WIN32
        cleanup_winsock();
#endif
        return 1;
    }
    g_stats.min_latency_us.store(0, std::memory_order_relaxed);
    g_stats.max_latency_us.store(0, std::memory_order_relaxed);
    g_stats.running.store(true, std::memory_order_relaxed);
    g_stats.latency_count.store(0, std::memory_order_relaxed);

    /* 创建进度线程 (jthread 自动管理) */
    std::jthread progress_tid(progress_thread);

    /* 创建工作线程 (RAII: jthread 析构自动 join) */
    g_stats.start_time = std::chrono::steady_clock::now();

    workers.reserve(static_cast<size_t>(g_config.threads));
    for (int i = 0; i < g_config.threads; i++) {
        workers.emplace_back(worker_thread);
    }

    /* 等待指定时间 (跨平台 chrono) */
    std::this_thread::sleep_for(std::chrono::seconds(g_config.duration_sec));

    /* 停止所有线程 (jthread::request_stop() 隐式调用) */
    g_stats.end_time = std::chrono::steady_clock::now();

    /* workers 和 progress_tid 析构时自动 join */

    std::free(g_stats.latencies);
    g_stats.latencies = nullptr;

    /* 生成报告 */
    print_report();

#ifdef _WIN32
    cleanup_winsock();
#endif

    return 0;
}
