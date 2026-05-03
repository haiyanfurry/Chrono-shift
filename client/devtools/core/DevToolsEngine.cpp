/**
 * DevToolsEngine.cpp — 开发者模式后端引擎实现
 *
 * 将 CLI 命令注册表封装为 C++ 引擎，
 * 支持 HTTP API / IPC / 直接调用三种执行路径。
 *
 * 注意：包含路径在 CMake 中通过 include_directories 配置，
 * 此处使用相对于 client/src/ 的路径，实际构建时需添加：
 *   ${CMAKE_CURRENT_SOURCE_DIR}/src
 *   ${CMAKE_CURRENT_SOURCE_DIR}/devtools/cli
 */
#include "DevToolsEngine.h"
#include "DevToolsHttpApi.h"

#include <cstring>
#include <cstdio>
#include <sstream>
#include <vector>
#include <algorithm>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#include <windows.h>
#endif

/* CLI 共享头文件 (来自 devtools/cli/) */
#include "devtools_cli.h"               // g_config, g_command_table, init_commands()

/* AppContext 及其子模块 (来自 src/) */
#include "app/AppContext.h"
#include "app/ClientHttpServer.h"
#include "app/IpcBridge.h"
#include "network/NetworkClient.h"
#include "storage/LocalStorage.h"
#include "storage/SessionManager.h"
#include "security/CryptoEngine.h"

#include "util/Logger.h"

/* ============================================================
 * 前向声明 — 内部工具函数
 * ============================================================ */

static std::string get_network_status_json();
static std::string get_storage_list_json();

/* ============================================================
 * 命名空间
 * ============================================================ */
namespace chrono {
namespace client {
namespace devtools {

/* ============================================================
 * 静态成员
 * ============================================================ */

std::ostringstream* DevToolsEngine::s_capture_stream = nullptr;

/* ============================================================
 * 单例
 * ============================================================ */

DevToolsEngine& DevToolsEngine::instance()
{
    static DevToolsEngine engine;
    return engine;
}

DevToolsEngine::DevToolsEngine()
{
}

DevToolsEngine::~DevToolsEngine()
{
    shutdown();
}

/* ============================================================
 * 生命周期
 * ============================================================ */

int DevToolsEngine::init(app::AppContext& ctx)
{
    if (initialized_.exchange(true)) {
        LOG_WARN("DevToolsEngine 重复初始化");
        return 0;
    }

    ctx_ = &ctx;

    /* 初始化 CLI 全局配置 */
    config_init_defaults();

    /* 从 AppContext 同步配置 */
    const auto& cfg = ctx.config();
    strncpy(g_config.host, cfg.server_host.c_str(), sizeof(g_config.host) - 1);
    g_config.port = cfg.server_port;

    /* 同步存储路径 */
    {
        auto& storage = ctx.storage();
        (void)storage; // 未来可从 storage 获取路径
    }

    /* 初始化 Winsock (独立 CLI 也需要) */
#ifdef _WIN32
    static WSADATA wsa;
    static bool wsa_inited = false;
    if (!wsa_inited) {
        WSAStartup(MAKEWORD(2, 2), &wsa);
        wsa_inited = true;
    }
#endif

    /* 注册内置命令 (与 CLI main.c 一致) */
    extern "C" int cmd_help(void);
    register_command("help", "显示帮助信息", "help", (CommandHandler)cmd_help);

    /* 注册所有命令模块 (调用 init_commands() — 声明在 devtools_cli.h 或 init_commands.c) */
    extern void init_commands(void);
    init_commands();

    LOG_INFO("DevToolsEngine 初始化完成, 已注册 %d 个命令", g_command_count);
    return 0;
}

void DevToolsEngine::shutdown()
{
    if (!initialized_.exchange(false)) {
        return;
    }

    /* 清理命令注册表 */
    g_command_count = 0;
    memset(g_command_table, 0, sizeof(g_command_table));

    ctx_ = nullptr;
    enabled_ = false;

#ifdef _WIN32
    WSACleanup();
#endif

    LOG_INFO("DevToolsEngine 已关闭");
}

void DevToolsEngine::set_dev_mode_enabled(bool enabled)
{
    enabled_ = enabled;
    LOG_INFO("开发者模式: %s", enabled ? "启用" : "禁用");
}

/* ============================================================
 * 输出捕获
 * ============================================================ */

int DevToolsEngine::output_capture(const char* buf, int len)
{
    if (s_capture_stream && buf && len > 0) {
        s_capture_stream->write(buf, len);
    }
    return len;
}

/* ============================================================
 * CLI 命令执行
 * ============================================================ */

std::string DevToolsEngine::execute_command(const std::string& cmd_line)
{
    if (cmd_line.empty()) {
        return "";
    }

    /* 分词 */
    std::vector<char*> tokens;
    std::string mutable_cmd = cmd_line;
    char* token = std::strtok(&mutable_cmd[0], " \t");
    while (token && tokens.size() < 64) {
        tokens.push_back(token);
        token = std::strtok(nullptr, " \t");
    }

    if (tokens.empty()) {
        return "";
    }

    return execute_command(static_cast<int>(tokens.size()), tokens.data());
}

std::string DevToolsEngine::execute_command(int argc, char** argv)
{
    if (argc < 1 || !argv || !argv[0]) {
        return "";
    }

    /* 特殊处理 help / exit */
    if (std::strcmp(argv[0], "help") == 0) {
        std::ostringstream oss;
        oss << "\n可用命令:\n";
        for (int i = 0; i < g_command_count; i++) {
            char buf[256];
            snprintf(buf, sizeof(buf), "  %-20s %s\n",
                     g_command_table[i].name,
                     g_command_table[i].description);
            oss << buf;
        }
        oss << "\n输入 '<命令> help' 查看详细用法\n";
        return oss.str();
    }

    /* 查找命令 */
    CommandHandler handler = find_command(argv[0]);
    if (!handler) {
        std::string msg = "未知命令: ";
        msg += argv[0];
        msg += " (输入 help 查看可用命令)\n";
        return msg;
    }

    /* 捕获 stdout 输出 */
    std::ostringstream capture;
    s_capture_stream = &capture;

    /* 重定向 stdout 到捕获缓冲区 (使用文件描述符重定向) */
    fflush(stdout);

#ifdef _WIN32
    int old_stdout_fd = _dup(_fileno(stdout));
    // 简化方案：使用临时文件捕获 stdout 输出
    char tmpfile_path[MAX_PATH] = {};
    char tmpdir[MAX_PATH] = {};
    GetTempPathA(MAX_PATH, tmpdir);
    GetTempFileNameA(tmpdir, "dev", 0, tmpfile_path);
    FILE* tmp_fp = fopen(tmpfile_path, "w+");
    if (tmp_fp) {
        int tmp_fd = _fileno(tmp_fp);
        _dup2(tmp_fd, _fileno(stdout));
    }
#else
    FILE* old_stdout = stdout;
    char tmpfile_path[] = "/tmp/devtools_capture_XXXXXX";
    int tmp_fd = mkstemp(tmpfile_path);
    if (tmp_fd >= 0) {
        int old_fd = dup(fileno(stdout));
        dup2(tmp_fd, fileno(stdout));
        close(tmp_fd);
    }
#endif

    /* 执行命令 */
    int ret = handler(argc, argv);

    /* 恢复 stdout */
    fflush(stdout);

#ifdef _WIN32
    if (tmp_fp) {
        fflush(tmp_fp);
        fseek(tmp_fp, 0, SEEK_SET);
        char read_buf[4096];
        size_t nread;
        while ((nread = fread(read_buf, 1, sizeof(read_buf), tmp_fp)) > 0) {
            capture.write(read_buf, nread);
        }
        fclose(tmp_fp);
        remove(tmpfile_path);
        _dup2(old_stdout_fd, _fileno(stdout));
        _close(old_stdout_fd);
    }
#else
    if (tmp_fd >= 0) {
        FILE* tmp_fp = fdopen(tmp_fd, "r");
        if (tmp_fp) {
            char read_buf[4096];
            size_t nread;
            while ((nread = fread(read_buf, 1, sizeof(read_buf), tmp_fp)) > 0) {
                capture.write(read_buf, nread);
            }
            fclose(tmp_fp);
        }
        dup2(old_fd, fileno(stdout));
        close(old_fd);
        unlink(tmpfile_path);
    }
#endif

    s_capture_stream = nullptr;

    if (ret != 0) {
        capture << "[-] 命令返回: " << ret << "\n";
    }

    return capture.str();
}

/* ============================================================
 * 命令查询
 * ============================================================ */

std::string DevToolsEngine::get_commands_json() const
{
    std::ostringstream json;
    json << "{\"commands\":[";
    for (int i = 0; i < g_command_count; i++) {
        if (i > 0) json << ",";
        json << "{"
             << "\"name\":\"" << g_command_table[i].name << "\","
             << "\"desc\":\"" << g_command_table[i].description << "\","
             << "\"usage\":\"" << g_command_table[i].usage << "\""
             << "}";
    }
    json << "],\"count\":" << g_command_count << "}";
    return json.str();
}

std::string DevToolsEngine::get_status_json() const
{
    std::ostringstream json;
    json << "{"
         << "\"enabled\":" << (enabled_ ? "true" : "false") << ","
         << "\"initialized\":" << (initialized_ ? "true" : "false") << ","
         << "\"host\":\"" << g_config.host << "\","
         << "\"port\":" << g_config.port << ","
         << "\"use_tls\":" << (g_config.use_tls ? "true" : "false") << ","
         << "\"logged_in\":" << (g_config.session_logged_in ? "true" : "false") << ","
         << "\"ws_connected\":" << (g_config.ws_connected ? "true" : "false") << ","
         << "\"command_count\":" << g_command_count
         << "}";
    return json.str();
}

/* ============================================================
 * HTTP API 路由注册 — 委托给 DevToolsHttpApi
 * ============================================================ */

void DevToolsEngine::register_http_routes(app::ClientHttpServer& http_server)
{
    if (!http_api_) {
        http_api_ = std::make_unique<DevToolsHttpApi>(*this);
    }
    http_api_->register_routes(http_server);
    LOG_INFO("DevToolsEngine: HTTP 路由已注册 (%s)",
             app::ClientHttpServer::kDevToolRoutePrefix);
}

void DevToolsEngine::unregister_http_routes(app::ClientHttpServer& http_server)
{
    if (http_api_) {
        http_api_->unregister_routes(http_server);
    }
    LOG_INFO("DevToolsEngine: HTTP 路由已注销");
}

/* ============================================================
 * IPC 消息处理器注册
 * ============================================================ */

void DevToolsEngine::register_ipc_handlers(app::IpcBridge& ipc)
{
    using namespace app;

    /* 0xC0: DEV_TOOLS_ENABLE — 启用/禁用开发者模式 */
    ipc.register_handler(
        static_cast<IpcMessageType>(0xC0),
        [this](IpcMessageType /*type*/, const std::string& json_data)
        {
            bool enable = (json_data.find("\"enable\":true") != std::string::npos);
            set_dev_mode_enabled(enable);

            /* 发送通知到前端 */
            std::string notify =
                "{\"type\":\"devtools_status\",\"enabled\":"
                + std::string(enable ? "true" : "false") + "}";

            if (ctx_) {
                auto& webview = ctx_->webview();
                (void)webview; /* WebView 发送在 IpcBridge::send_to_js 中处理 */
            }
        });

    /* 0xC1: DEV_TOOLS_EXEC — 执行命令 */
    ipc.register_handler(
        static_cast<IpcMessageType>(0xC1),
        [this](IpcMessageType /*type*/, const std::string& json_data)
        {
            /* 提取 cmd 字段 (简单 JSON 解析) */
            std::string cmd;
            auto pos = json_data.find("\"cmd\"");
            if (pos != std::string::npos) {
                auto vstart = json_data.find('"', pos + 5);
                if (vstart != std::string::npos) {
                    auto vend = json_data.find('"', vstart + 1);
                    if (vend != std::string::npos) {
                        cmd = json_data.substr(vstart + 1, vend - vstart - 1);
                    }
                }
            }

            std::string result;
            if (!cmd.empty()) {
                result = execute_command(cmd);
            } else {
                result = "Missing 'cmd' field";
            }

            /* 通过 IpcBridge 发回结果 (需要 WebView 指针) */
            if (ctx_) {
                std::string response =
                    "{\"type\":\"devtools_exec_result\",\"cmd\":\""
                    + cmd + "\",\"output\":\"" + result + "\"}";
                /* send_to_js 需要 WebView 指针，由 AppContext 管理 */
                (void)response;
            }
        });

    /* 0xC2: DEV_TOOLS_LOG — C++ → JS 日志推送 (仅注册类型，用于权限验证) */
    /* 0xC3: DEV_TOOLS_NETWORK_EVENT — C++ → JS 网络事件推送 (同上) */

    registered_ipc_types_ = {0xC0, 0xC1, 0xC2, 0xC3};
    LOG_INFO("DevToolsEngine: IPC 处理器已注册 (0xC0-0xC3)");
}

void DevToolsEngine::unregister_ipc_handlers(app::IpcBridge& /*ipc*/)
{
    /* IpcBridge 当前不支持按类型注销，重置列表 */
    registered_ipc_types_.clear();
    LOG_INFO("DevToolsEngine: IPC 处理器已注销");
}

/* ============================================================
 * 内部工具 — 网络状态 JSON
 * ============================================================ */

static std::string get_network_status_json()
{
    /* 使用 NetworkClient 获取连接状态 */
    std::ostringstream json;
    json << "{"
         << "\"host\":\"" << g_config.host << "\","
         << "\"port\":" << g_config.port << ","
         << "\"use_tls\":" << (g_config.use_tls ? "true" : "false") << ","
         << "\"ws_connected\":" << (g_config.ws_connected ? "true" : "false") << ","
         << "\"logged_in\":" << (g_config.session_logged_in ? "true" : "false")
         << "}";
    return json.str();
}

/* ============================================================
 * 内部工具 — 存储列表 JSON
 * ============================================================ */

static std::string get_storage_list_json()
{
    /* 使用 LocalStorage 枚举存储内容 */
    std::ostringstream json;
    json << "{\"keys\":[";
    /* LocalStorage 暂未提供枚举 API，返回占位 */
    json << "],\"path\":\"" << g_config.storage_path << "\"}";
    return json.str();
}

/* ============================================================
 * C linkage 帮助函数 (供 CLI help 命令使用)
 * ============================================================ */

extern "C" int cmd_help(void)
{
    printf("\n");
    printf("  ╔══════════════════════════════════════════════════════════╗\n");
    printf("  ║      Chrono-shift 开发者模式 CLI v0.1.0                 ║\n");
    printf("  ╚══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    printf("可用命令:\n");
    for (int i = 0; i < g_command_count; i++) {
        printf("  %-20s %s\n", g_command_table[i].name,
               g_command_table[i].description);
    }
    printf("\n");
    printf("配置:\n");
    printf("  当前服务器: %s:%d\n", g_config.host, g_config.port);
    printf("  协议:       %s\n", g_config.use_tls ? "HTTPS" : "HTTP");
    printf("  会话状态:   %s\n", g_config.session_logged_in ? "已登录" : "未登录");
    printf("\n");
    return 0;
}

} // namespace devtools
} // namespace client
} // namespace chrono
