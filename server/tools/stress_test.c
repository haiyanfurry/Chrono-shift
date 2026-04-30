/**
 * 墨竹 (Chrono-shift) 压力测试框架
 * 
 * 用途：评估服务器在高负载下的抗冲击能力
 * 特性：
 *   - 多线程并发请求
 *   - 可配置 QPS 目标、线程数、持续时间
 *   - 实时统计：QPS、延迟(P50/P95/P99)、错误率
 *   - 支持多种测试场景
 *   - Windows (WinSock2) + Linux (POSIX) 双平台
 *
 * 编译:
 *   Windows: cl stress_test.c /I../include /Fe:stress_test.exe /link ws2_32.lib
 *   Linux:   gcc stress_test.c -I../include -o stress_test -lpthread -lm
 *
 * 使用:
 *   stress_test --host 127.0.0.1 --port 8080 --threads 4 --qps 100 --duration 30
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#ifdef _WIN32
    #include <winsock2.h>
    #include <windows.h>
    #include <process.h>
    typedef HANDLE thread_t;
    #define THREAD_FUNC unsigned __stdcall
    #define THREAD_RETURN return 0
    #define usleep(us) Sleep((us) / 1000)
    #define strcasecmp _stricmp
#else
    #include <unistd.h>
    #include <pthread.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netdb.h>
    #include <arpa/inet.h>
    #include <signal.h>
    typedef pthread_t thread_t;
    #define THREAD_FUNC void*
    #define THREAD_RETURN return NULL
    #define SOCKET int
    #define INVALID_SOCKET (-1)
    #define SOCKET_ERROR (-1)
    #define closesocket(fd) close(fd)
#endif

/* ============================================================
   配置
   ============================================================ */

#define MAX_URL_LEN    512
#define MAX_HEADERS    64
#define MAX_HEADER_LEN 256
#define MAX_RESP_BUF   65536
#define MAX_THREADS    64
#define BENCHMARK_RUNS 5

/* 测试场景 */
typedef struct {
    const char* name;
    const char* method;
    const char* path;
    const char* body;
    const char* content_type;
} TestScenario;

static const TestScenario SCENARIOS[] = {
    {"健康检查",   "GET",    "/api/health",          NULL, NULL},
    {"用户注册",   "POST",   "/api/user/register",   "{\"username\":\"stress_test_user\",\"password\":\"Test123456\",\"nickname\":\"压力测试\"}", "application/json"},
    {"用户登录",   "POST",   "/api/user/login",      "{\"username\":\"stress_test_user\",\"password\":\"Test123456\"}", "application/json"},
    {"获取模板",   "GET",    "/api/templates",       NULL, NULL},
    {"发送消息",   "POST",   "/api/message/send",    "{\"to_user_id\":\"test\",\"content\":\"压力测试消息\"}", "application/json"},
    {NULL, NULL, NULL, NULL, NULL}  /* 结束标记 */
};

/* 统计 */
typedef struct {
    volatile long long total_requests;
    volatile long long success_count;
    volatile long long error_count;
    volatile long long total_latency_us;    /* 总延迟（微秒） */
    volatile long long min_latency_us;
    volatile long long max_latency_us;
    double* latencies;                      /* 延迟样本数组 */
    volatile int latency_count;
    int latency_capacity;
    int running;
    long long start_time_ms;
    long long end_time_ms;
} Stats;

/* 命令行参数 */
typedef struct {
    char host[256];
    int  port;
    int  threads;
    int  qps_target;
    int  duration_sec;
    int  scenario_index;
    int  verbose;
} Config;

/* ============================================================
   全局变量
   ============================================================ */

static Config g_config = {
    .host = "127.0.0.1",
    .port = 8080,
    .threads = 4,
    .qps_target = 100,
    .duration_sec = 30,
    .scenario_index = 0,
    .verbose = 0
};

static Stats g_stats = {0};
static int g_running = 1;

/* ============================================================
   工具函数
   ============================================================ */

static long long get_time_ms(void) {
#ifdef _WIN32
    return (long long)GetTickCount64();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
#endif
}

static long long get_time_us(void) {
#ifdef _WIN32
    LARGE_INTEGER freq, count;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    return (count.QuadPart * 1000000) / freq.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
#endif
}

#ifdef _WIN32
static int init_winsock(void) {
    WSADATA wsa;
    return WSAStartup(MAKEWORD(2, 2), &wsa);
}

static void cleanup_winsock(void) {
    WSACleanup();
}
#endif

/* 安全 float -> int 转换 */
static int safe_round(double val) {
    if (val < 0) return 0;
    if (val > 1e9) return (int)1e9;
    return (int)(val + 0.5);
}

/* ============================================================
   HTTP 请求
   ============================================================ */

static int http_request(
    const char* method, const char* path,
    const char* body, const char* content_type,
    char* response, int resp_size,
    long long* latency_us
) {
    SOCKET sock;
    struct sockaddr_in addr;
    struct hostent* host;
    char request[4096];
    char header_buf[4096];
    int ret = -1;
    long long t0, t1;

    /* 创建 socket */
    sock = (int)socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) return -1;

    /* DNS 解析 */
    host = gethostbyname(g_config.host);
    if (!host) {
        closesocket(sock);
        return -1;
    }

    /* 连接 */
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)g_config.port);
    memcpy(&addr.sin_addr, host->h_addr_list[0], (size_t)host->h_length);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        closesocket(sock);
        return -1;
    }

    /* 构建 HTTP 请求 */
    if (body && content_type) {
        snprintf(request, sizeof(request),
            "%s %s HTTP/1.1\r\n"
            "Host: %s:%d\r\n"
            "Content-Type: %s\r\n"
            "Content-Length: %zu\r\n"
            "Connection: close\r\n"
            "\r\n"
            "%s",
            method, path,
            g_config.host, g_config.port,
            content_type,
            strlen(body),
            body
        );
    } else {
        snprintf(request, sizeof(request),
            "%s %s HTTP/1.1\r\n"
            "Host: %s:%d\r\n"
            "Connection: close\r\n"
            "\r\n",
            method, path,
            g_config.host, g_config.port
        );
    }

    /* 计时开始 */
    t0 = get_time_us();

    /* 发送请求 */
    if (send(sock, request, (int)strlen(request), 0) == SOCKET_ERROR) {
        closesocket(sock);
        return -1;
    }

    /* 接收响应（仅读取前 resp_size 字节） */
    {
        int total = 0;
        int n;
        while (total < resp_size - 1) {
            n = (int)recv(sock, response + total, (int)(resp_size - 1 - total), 0);
            if (n <= 0) break;
            total += n;
        }
        response[total] = '\0';
    }

    /* 计时结束 */
    t1 = get_time_us();

    if (latency_us) *latency_us = t1 - t0;

    /* 检查 HTTP 状态码 */
    if (strncmp(response, "HTTP/1.1 200", 12) == 0 ||
        strncmp(response, "HTTP/1.0 200", 12) == 0) {
        ret = 0;  /* 成功 */
    } else if (strstr(response, "HTTP/1.1") || strstr(response, "HTTP/1.0")) {
        ret = 1;  /* 非 200 但有效响应 */
    } else {
        ret = -1; /* 无效响应 */
    }

    closesocket(sock);
    return ret;
}

/* ============================================================
   工作线程
   ============================================================ */

static THREAD_FUNC worker_thread(void* arg) {
    (void)arg;
    char resp[MAX_RESP_BUF];
    const TestScenario* scenario = &SCENARIOS[g_config.scenario_index];
    long long interval_us = (g_config.qps_target > 0) ? (1000000LL / g_config.qps_target) : 0;
    long long next_wake = get_time_us();

    while (g_running) {
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

        /* 更新统计 */
        if (result == 0) {
            InterlockedIncrement(&g_stats.success_count);
        } else {
            InterlockedIncrement(&g_stats.error_count);
        }
        InterlockedIncrement(&g_stats.total_requests);
        InterlockedExchangeAdd64(&g_stats.total_latency_us, latency);

        /* 记录延迟样本 */
        if (g_stats.latency_count < g_stats.latency_capacity) {
            int idx = InterlockedIncrement(&g_stats.latency_count) - 1;
            if (idx >= 0 && idx < g_stats.latency_capacity) {
                g_stats.latencies[idx] = (double)latency;
            }
        }

        /* 更新最大/最小延迟 */
        if (latency < g_stats.min_latency_us || g_stats.min_latency_us == 0)
            InterlockedExchange64(&g_stats.min_latency_us, latency);
        if (latency > g_stats.max_latency_us)
            InterlockedExchange64(&g_stats.max_latency_us, latency);

        /* QPS 控制 */
        if (interval_us > 0) {
            next_wake += interval_us;
            long long now = get_time_us();
            if (next_wake > now && (next_wake - now) < 1000000) {
                usleep((unsigned int)(next_wake - now));
            } else if (next_wake <= now) {
                next_wake = now;
            }
        }
    }

    THREAD_RETURN;
}

/* ============================================================
   比较函数（用于 qsort）
   ============================================================ */

static int cmp_double(const void* a, const void* b) {
    double da = *(const double*)a;
    double db = *(const double*)b;
    if (da < db) return -1;
    if (da > db) return 1;
    return 0;
}

/* ============================================================
   打印帮助
   ============================================================ */

static void print_usage(void) {
    printf("墨竹 (Chrono-shift) 压力测试框架\n");
    printf("用法: stress_test [选项]\n\n");
    printf("选项:\n");
    printf("  --host <ip>       服务器地址 (默认: 127.0.0.1)\n");
    printf("  --port <port>     服务器端口 (默认: 8080)\n");
    printf("  --threads <n>     并发线程数 (默认: 4, 最大: %d)\n", MAX_THREADS);
    printf("  --qps <n>         QPS 目标 (默认: 100)\n");
    printf("  --duration <s>    测试持续时间 (秒) (默认: 30)\n");
    printf("  --scenario <n>    测试场景索引 (默认: 0)\n");
    printf("  --list-scenarios  列出所有测试场景\n");
    printf("  --verbose         详细输出\n");
    printf("  --help            显示此帮助\n\n");
    printf("测试场景:\n");
    for (int i = 0; SCENARIOS[i].name; i++) {
        printf("  [%d] %s (%s %s)\n", i, SCENARIOS[i].name, SCENARIOS[i].method, SCENARIOS[i].path);
    }
}

static void print_scenarios(void) {
    printf("可用的测试场景:\n");
    for (int i = 0; SCENARIOS[i].name; i++) {
        printf("  [%d] %s\n", i, SCENARIOS[i].name);
        printf("      方法: %s\n", SCENARIOS[i].method);
        printf("      路径: %s\n", SCENARIOS[i].path);
        printf("      请求体: %s\n", SCENARIOS[i].body ? SCENARIOS[i].body : "(无)");
        printf("\n");
    }
}

/* ============================================================
   统计汇总
   ============================================================ */

static void print_report(void) {
    long long elapsed_ms = g_stats.end_time_ms - g_stats.start_time_ms;
    double elapsed_sec = elapsed_ms / 1000.0;
    long long total = g_stats.total_requests;
    double actual_qps = (elapsed_sec > 0) ? (total / elapsed_sec) : 0;
    double avg_latency_us = (total > 0) ? ((double)g_stats.total_latency_us / total) : 0;

    printf("\n");
    printf("========================================\n");
    printf("  墨竹 压力测试报告\n");
    printf("========================================\n");
    printf("\n");

    /* 测试配置 */
    printf("【测试配置】\n");
    printf("  服务器:      %s:%d\n", g_config.host, g_config.port);
    printf("  场景:        %s (%s %s)\n",
        SCENARIOS[g_config.scenario_index].name,
        SCENARIOS[g_config.scenario_index].method,
        SCENARIOS[g_config.scenario_index].path);
    printf("  并发线程:    %d\n", g_config.threads);
    printf("  QPS 目标:    %d req/s\n", g_config.qps_target);
    printf("  计划时长:    %d 秒\n", g_config.duration_sec);
    printf("  实际时长:    %.2f 秒\n", elapsed_sec);
    printf("\n");

    /* 吞吐量 */
    printf("【吞吐量】\n");
    printf("  总请求数:    %lld\n", total);
    printf("  成功请求:    %lld\n", (long long)g_stats.success_count);
    printf("  失败请求:    %lld\n", (long long)g_stats.error_count);
    printf("  成功率:      %.2f%%\n",
        (total > 0) ? (g_stats.success_count * 100.0 / total) : 0);
    printf("  实际 QPS:    %.2f req/s\n", actual_qps);
    printf("  目标达成率:  %.2f%%\n",
        (g_config.qps_target > 0) ? (actual_qps * 100.0 / g_config.qps_target) : 0);
    printf("\n");

    /* 延迟 */
    printf("【延迟】\n");
    printf("  平均延迟:    %.2f ms\n", avg_latency_us / 1000.0);
    printf("  最小延迟:    %.2f ms\n", (double)g_stats.min_latency_us / 1000.0);
    printf("  最大延迟:    %.2f ms\n", (double)g_stats.max_latency_us / 1000.0);

    /* 百分位延迟 */
    if (g_stats.latency_count > 0) {
        qsort(g_stats.latencies, (size_t)g_stats.latency_count, sizeof(double), cmp_double);
        int count = g_stats.latency_count;
        printf("  P50 延迟:    %.2f ms\n", g_stats.latencies[count * 50 / 100] / 1000.0);
        printf("  P90 延迟:    %.2f ms\n", g_stats.latencies[count * 90 / 100] / 1000.0);
        printf("  P95 延迟:    %.2f ms\n", g_stats.latencies[count * 95 / 100] / 1000.0);
        printf("  P99 延迟:    %.2f ms\n", g_stats.latencies[count * 99 / 100] / 1000.0);
    }

    /* 抗冲击评估 */
    printf("\n");
    printf("【抗冲击能力评估】\n");
    double error_rate = (total > 0) ? (g_stats.error_count * 100.0 / total) : 0;
    if (error_rate > 20.0) {
        printf("  等级: ❌ 不合格 (错误率 %.2f%% > 20%%)\n", error_rate);
        printf("  建议: 检查服务器资源限制或优化请求处理逻辑\n");
    } else if (error_rate > 5.0) {
        printf("  等级: ⚠️ 一般 (错误率 %.2f%% > 5%%)\n", error_rate);
        printf("  建议: 适当增加服务器资源或进行代码级优化\n");
    } else if (error_rate > 1.0) {
        printf("  等级: ✅ 良好 (错误率 %.2f%%)\n", error_rate);
        printf("  建议: 少量优化即可达到优秀\n");
    } else {
        printf("  等级: 🏆 优秀 (错误率 %.2f%%)\n", error_rate);
        printf("  说明: 框架在高负载下表现稳定\n");
    }

    double target_ratio = (g_config.qps_target > 0) ? (actual_qps / g_config.qps_target) : 1;
    if (target_ratio < 0.5) {
        printf("  QPS 达成:    ❌ 未达标 (仅达成目标的 %.1f%%)\n", target_ratio * 100.0);
    } else if (target_ratio < 0.8) {
        printf("  QPS 达成:    ⚠️ 部分达标 (达成目标的 %.1f%%)\n", target_ratio * 100.0);
    } else if (target_ratio < 1.0) {
        printf("  QPS 达成:    ✅ 接近目标 (达成目标的 %.1f%%)\n", target_ratio * 100.0);
    } else {
        printf("  QPS 达成:    🏆 超越目标 (达成目标的 %.1f%%)\n", target_ratio * 100.0);
    }

    printf("\n");
    printf("========================================\n");
    printf("\n");

    /* 保存报告到文件 */
    FILE* fp = fopen("reports/stress_test_report.md", "w");
    if (fp) {
        fprintf(fp, "# 墨竹 压力测试报告\n\n");
        fprintf(fp, "## 测试配置\n\n");
        fprintf(fp, "| 参数 | 值 |\n");
        fprintf(fp, "|------|-----|\n");
        fprintf(fp, "| 服务器 | %s:%d |\n", g_config.host, g_config.port);
        fprintf(fp, "| 场景 | %s |\n", SCENARIOS[g_config.scenario_index].name);
        fprintf(fp, "| 并发线程 | %d |\n", g_config.threads);
        fprintf(fp, "| QPS 目标 | %d req/s |\n", g_config.qps_target);
        fprintf(fp, "| 测试时长 | %.2f 秒 |\n", elapsed_sec);
        fprintf(fp, "\n");
        fprintf(fp, "## 测试结果\n\n");
        fprintf(fp, "| 指标 | 值 |\n");
        fprintf(fp, "|------|-----|\n");
        fprintf(fp, "| 总请求数 | %lld |\n", total);
        fprintf(fp, "| 成功率 | %.2f%% |\n", (total > 0) ? (g_stats.success_count * 100.0 / total) : 0);
        fprintf(fp, "| 实际 QPS | %.2f req/s |\n", actual_qps);
        fprintf(fp, "| 平均延迟 | %.2f ms |\n", avg_latency_us / 1000.0);
        fprintf(fp, "| P50 延迟 | %.2f ms |\n", g_stats.latency_count > 0 ? g_stats.latencies[g_stats.latency_count * 50 / 100] / 1000.0 : 0);
        fprintf(fp, "| P99 延迟 | %.2f ms |\n", g_stats.latency_count > 0 ? g_stats.latencies[g_stats.latency_count * 99 / 100] / 1000.0 : 0);
        fprintf(fp, "| 错误率 | %.2f%% |\n", error_rate);
        fprintf(fp, "| 抗冲击等级 | %s |\n",
            error_rate > 20.0 ? "❌ 不合格" :
            error_rate > 5.0  ? "⚠️ 一般" :
            error_rate > 1.0  ? "✅ 良好" : "🏆 优秀");
        fprintf(fp, "\n");
        fclose(fp);
        printf("  报告已保存至: reports/stress_test_report.md\n");
    }
}

/* ============================================================
   进度显示线程
   ============================================================ */

static THREAD_FUNC progress_thread(void* arg) {
    (void)arg;
    int last_total = 0;
    int dots = 0;

    printf("\n正在执行压力测试...\n");
    printf("  目标: %d QPS, %d 线程, %d 秒\n\n",
        g_config.qps_target, g_config.threads, g_config.duration_sec);

    while (g_running) {
        long long elapsed = get_time_ms() - g_stats.start_time_ms;
        int seconds = (int)(elapsed / 1000);
        int current_total = (int)g_stats.total_requests;
        int qps_now = current_total - last_total;
        last_total = current_total;

        printf("\r  [%02d:%02d] 请求: %d | QPS: %d | 成功: %lld | 失败: %lld",
            seconds / 60, seconds % 60,
            current_total,
            qps_now,
            (long long)g_stats.success_count,
            (long long)g_stats.error_count);

        fflush(stdout);
        dots++;

#ifdef _WIN32
        Sleep(1000);
#else
        sleep(1);
#endif
    }
    printf("\n");
    THREAD_RETURN;
}

/* ============================================================
   主函数
   ============================================================ */

int main(int argc, char* argv[]) {
    thread_t* workers = NULL;
    thread_t progress_tid;
    int i;

#ifdef _WIN32
    if (init_winsock() != 0) {
        fprintf(stderr, "错误: WinSock 初始化失败\n");
        return 1;
    }
#endif

    /* 解析命令行参数 */
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--host") == 0 && i + 1 < argc) {
            strncpy(g_config.host, argv[++i], sizeof(g_config.host) - 1);
        } else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            g_config.port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--threads") == 0 && i + 1 < argc) {
            g_config.threads = atoi(argv[++i]);
            if (g_config.threads < 1) g_config.threads = 1;
            if (g_config.threads > MAX_THREADS) g_config.threads = MAX_THREADS;
        } else if (strcmp(argv[i], "--qps") == 0 && i + 1 < argc) {
            g_config.qps_target = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--duration") == 0 && i + 1 < argc) {
            g_config.duration_sec = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--scenario") == 0 && i + 1 < argc) {
            g_config.scenario_index = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--list-scenarios") == 0) {
            print_scenarios();
#ifdef _WIN32
            cleanup_winsock();
#endif
            return 0;
        } else if (strcmp(argv[i], "--verbose") == 0) {
            g_config.verbose = 1;
        } else if (strcmp(argv[i], "--help") == 0) {
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
    if (g_config.scenario_index < 0 || g_config.scenario_index >= scenario_count) {
        fprintf(stderr, "错误: 无效的场景索引 %d (可用: 0-%d)\n",
            g_config.scenario_index, scenario_count - 1);
#ifdef _WIN32
        cleanup_winsock();
#endif
        return 1;
    }

    printf("========================================\n");
    printf("  墨竹 (Chrono-shift) 压力测试\n");
    printf("========================================\n");
    printf("  服务器:  %s:%d\n", g_config.host, g_config.port);
    printf("  场景:    %s\n", SCENARIOS[g_config.scenario_index].name);
    printf("  线程数:  %d\n", g_config.threads);
    printf("  QPS:     %d\n", g_config.qps_target);
    printf("  时长:    %d 秒\n", g_config.duration_sec);
    printf("========================================\n\n");

    /* 初始化统计区 */
    g_stats.latency_capacity = g_config.qps_target * g_config.duration_sec * 2;
    if (g_stats.latency_capacity < 10000) g_stats.latency_capacity = 10000;
    g_stats.latencies = (double*)calloc((size_t)g_stats.latency_capacity, sizeof(double));
    if (!g_stats.latencies) {
        fprintf(stderr, "错误: 内存不足\n");
#ifdef _WIN32
        cleanup_winsock();
#endif
        return 1;
    }
    g_stats.min_latency_us = 0;
    g_stats.max_latency_us = 0;
    g_stats.running = 1;

    /* 创建进度线程 */
#ifdef _WIN32
    progress_tid = (HANDLE)_beginthreadex(NULL, 0, progress_thread, NULL, 0, NULL);
#else
    pthread_create(&progress_tid, NULL, progress_thread, NULL);
#endif

    /* 创建工作线程 */
    workers = (thread_t*)calloc((size_t)g_config.threads, sizeof(thread_t));
    if (!workers) {
        fprintf(stderr, "错误: 内存不足\n");
        free(g_stats.latencies);
#ifdef _WIN32
        cleanup_winsock();
#endif
        return 1;
    }

    g_stats.start_time_ms = get_time_ms();

    for (i = 0; i < g_config.threads; i++) {
#ifdef _WIN32
        workers[i] = (HANDLE)_beginthreadex(NULL, 0, worker_thread, NULL, 0, NULL);
#else
        pthread_create(&workers[i], NULL, worker_thread, NULL);
#endif
    }

    /* 等待指定时间 */
#ifdef _WIN32
    Sleep((DWORD)g_config.duration_sec * 1000);
#else
    sleep((unsigned int)g_config.duration_sec);
#endif

    /* 停止所有线程 */
    g_running = 0;
    g_stats.end_time_ms = get_time_ms();

#ifdef _WIN32
    WaitForMultipleObjects((DWORD)g_config.threads, workers, TRUE, 5000);
    for (i = 0; i < g_config.threads; i++) {
        if (workers[i] != NULL) CloseHandle(workers[i]);
    }
    if (progress_tid != NULL) CloseHandle(progress_tid);
#else
    for (i = 0; i < g_config.threads; i++) {
        pthread_join(workers[i], NULL);
    }
    pthread_join(progress_tid, NULL);
#endif

    free(workers);

    /* 生成报告 */
    print_report();

    /* 清理 */
    free(g_stats.latencies);
#ifdef _WIN32
    cleanup_winsock();
#endif

    return 0;
}
