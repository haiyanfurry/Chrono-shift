/**
 * Chrono-shift C++ 服务端主入口
 * C++17 重构版 — 启动 HTTP/TLS 服务器
 */
#include "util/Logger.h"
#include "util/Protocol.h"
#include "http/HttpServer.h"
#include "http/HttpTypes.h"
#include "http/HttpParser.h"
#include "tls/TlsContext.h"
#include "db/Database.h"
#include "ffi/RustBridge.h"
#include "security/SecurityManager.h"
#include "handler/UserHandler.h"
#include "handler/MessageHandler.h"
#include "handler/OAuthHandler.h"
#include "json/JsonParser.h"
#include "ws/WebSocket.h"

#include <iostream>
#include <csignal>
#include <cstdlib>

// ============================================================
// 全局服务器实例 (用于信号处理)
// ============================================================
static chrono::http::HttpServer* g_server = nullptr;

// ============================================================
// 信号处理
// ============================================================
static void signal_handler(int sig)
{
    LOG_INFO("[MAIN] Received signal %d, shutting down...", sig);
    if (g_server) {
        g_server->stop();
    }
}

// ============================================================
// 主函数
// ============================================================
int main(int argc, char* argv[])
{
    // === 配置 ===
    std::string host = "0.0.0.0";
    int port = 9443;
    std::string data_path = "./data";
    std::string cert_path = "./certs/cert.pem";
    std::string key_path  = "./certs/key.pem";
    int workers = 4;
    bool use_tls = true;

    // 简单命令行解析
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            port = std::atoi(argv[++i]);
        } else if (arg == "--host" && i + 1 < argc) {
            host = argv[++i];
        } else if (arg == "--data" && i + 1 < argc) {
            data_path = argv[++i];
        } else if (arg == "--cert" && i + 1 < argc) {
            cert_path = argv[++i];
        } else if (arg == "--key" && i + 1 < argc) {
            key_path = argv[++i];
        } else if (arg == "--workers" && i + 1 < argc) {
            workers = std::atoi(argv[++i]);
        } else if (arg == "--no-tls") {
            use_tls = false;
        } else if (arg == "--help") {
            std::cout << "Chrono-shift Server (C++17)" << std::endl;
            std::cout << "Usage: " << argv[0] << " [options]" << std::endl;
            std::cout << "  --port <port>         Port (default: 9443)" << std::endl;
            std::cout << "  --host <host>         Host (default: 0.0.0.0)" << std::endl;
            std::cout << "  --data <path>         Data directory (default: ./data)" << std::endl;
            std::cout << "  --cert <path>         TLS cert file (default: ./certs/cert.pem)" << std::endl;
            std::cout << "  --key <path>          TLS key file (default: ./certs/key.pem)" << std::endl;
            std::cout << "  --workers <n>         Worker threads (default: 4)" << std::endl;
            std::cout << "  --no-tls              Disable TLS" << std::endl;
            std::cout << "  --help                Show this help" << std::endl;
            return 0;
        }
    }

    // === 初始化 ===
    LOG_INFO("[MAIN] Chrono-shift Server (C++17) starting...");

    // 初始化 TLS
    chrono::tls::TlsContext::global_init();

    // 初始化 Rust 安全模块
    if (!chrono::ffi::RustBridge::init("./certs/")) {
        LOG_WARN("[MAIN] Rust security module init failed, continuing...");
    }
    LOG_INFO("[MAIN] Rust security module version: %s",
             chrono::ffi::RustBridge::version().c_str());

    // 初始化数据库
    chrono::db::Database db(data_path);
    if (!db.init()) {
        LOG_ERROR("[MAIN] Failed to initialize database at %s", data_path.c_str());
        return 1;
    }

    // === 创建处理器 ===
    chrono::handler::UserHandler user_handler(db);
    chrono::handler::MessageHandler msg_handler(db);
    chrono::handler::OAuthHandler oauth_handler(db);

    // 配置 OAuth 回调地址（前端 oauth_callback.html，用于 postMessage 通信）
    chrono::handler::OAuthConfig oauth_config;
    oauth_config.qq.redirect_uri       = "http://127.0.0.1:9010/oauth_callback.html";
    oauth_config.wechat.redirect_uri   = "http://127.0.0.1:9010/oauth_callback.html";
    oauth_config.base_url              = "https://127.0.0.1:" + std::to_string(port);
    oauth_handler.init_oauth_config(oauth_config);

    // === 创建服务器 ===
    chrono::http::ServerConfig config;
    config.host     = host;
    config.port     = port;
    config.worker_count = workers;
    config.enable_tls   = use_tls;
    config.tls_cert_path = cert_path;
    config.tls_key_path  = key_path;

    chrono::http::HttpServer server(config);
    g_server = &server;

    // === 注册路由 ===
    using namespace chrono::http;
    using namespace chrono::json;

    // 用户注册
    server.register_route(Method::kPost, "/api/user/register",
        [&user_handler](const Request& req, Response& res) {
            JsonParser parser;
            auto params = parser.parse(
                std::string(req.body.begin(), req.body.end()));
            if (!params || !params->is_object()) {
                res.set_status(400, "Bad Request").set_json(
                    build_error("无效的请求体").serialize());
                return;
            }
            auto result = user_handler.handle_register(*params);
            res.set_json(result.serialize());
        });

    // 用户登录
    server.register_route(Method::kPost, "/api/user/login",
        [&user_handler](const Request& req, Response& res) {
            JsonParser parser;
            auto params = parser.parse(
                std::string(req.body.begin(), req.body.end()));
            if (!params || !params->is_object()) {
                res.set_status(400, "Bad Request").set_json(
                    build_error("无效的请求体").serialize());
                return;
            }
            auto result = user_handler.handle_login(*params);
            res.set_json(result.serialize());
        });

    // 获取用户信息
    server.register_route(Method::kGet, "/api/user/profile",
        [&user_handler](const Request& req, Response& res) {
            JsonValue params = json_object();
            auto it = req.headers.find("X-User-Id");
            if (it != req.headers.end()) {
                params.object_insert("user_id", JsonValue(it->second));
            }
            auto result = user_handler.handle_get_profile(params);
            res.set_json(result.serialize());
        });

    // 搜索用户
    server.register_route(Method::kGet, "/api/user/search",
        [&user_handler](const Request& req, Response& res) {
            // 从 URL 查询参数提取 keyword (简化)
            JsonValue params = json_object();
            // TODO: 解析 query string
            auto result = user_handler.handle_search(params);
            res.set_json(result.serialize());
        });

    // 发送消息
    server.register_route(Method::kPost, "/api/message/send",
        [&msg_handler](const Request& req, Response& res) {
            JsonParser parser;
            auto params = parser.parse(
                std::string(req.body.begin(), req.body.end()));
            if (!params || !params->is_object()) {
                res.set_status(400, "Bad Request").set_json(
                    build_error("无效的请求体").serialize());
                return;
            }
            auto result = msg_handler.handle_send(*params);
            res.set_json(result.serialize());
        });

    // 获取消息历史
    server.register_route(Method::kGet, "/api/message/history",
        [&msg_handler](const Request& req, Response& res) {
            JsonValue params = json_object();
            // TODO: 从 query string 解析参数
            auto result = msg_handler.handle_get_history(params);
            res.set_json(result.serialize());
        });

    // QQ 登录
    server.register_route(Method::kPost, "/api/oauth/qq",
        [&oauth_handler](const Request& req, Response& res) {
            JsonParser parser;
            auto params = parser.parse(
                std::string(req.body.begin(), req.body.end()));
            if (!params || !params->is_object()) {
                res.set_status(400, "Bad Request").set_json(
                    build_error("无效的请求体").serialize());
                return;
            }
            auto result = oauth_handler.handle_qq_login(*params);
            res.set_json(result.serialize());
        });

    // 微信登录
    server.register_route(Method::kPost, "/api/oauth/wechat",
        [&oauth_handler](const Request& req, Response& res) {
            JsonParser parser;
            auto params = parser.parse(
                std::string(req.body.begin(), req.body.end()));
            if (!params || !params->is_object()) {
                res.set_status(400, "Bad Request").set_json(
                    build_error("无效的请求体").serialize());
                return;
            }
            auto result = oauth_handler.handle_wechat_login(*params);
            res.set_json(result.serialize());
        });

    // 邮箱登录
    server.register_route(Method::kPost, "/api/oauth/email/login",
        [&oauth_handler](const Request& req, Response& res) {
            JsonParser parser;
            auto params = parser.parse(
                std::string(req.body.begin(), req.body.end()));
            if (!params || !params->is_object()) {
                res.set_status(400, "Bad Request").set_json(
                    build_error("无效的请求体").serialize());
                return;
            }
            auto result = oauth_handler.handle_email_login(*params);
            res.set_json(result.serialize());
        });

    // 邮箱注册
    server.register_route(Method::kPost, "/api/oauth/email/register",
        [&oauth_handler](const Request& req, Response& res) {
            JsonParser parser;
            auto params = parser.parse(
                std::string(req.body.begin(), req.body.end()));
            if (!params || !params->is_object()) {
                res.set_status(400, "Bad Request").set_json(
                    build_error("无效的请求体").serialize());
                return;
            }
            auto result = oauth_handler.handle_email_register(*params);
            res.set_json(result.serialize());
        });

    // ============================================================
    // P9.2 新 OAuth 路由
    // ============================================================

    // 辅助函数：解析 URL 查询字符串为 JsonValue 对象
    // 输入: "code=xxx&state=yyy"
    // 输出: { "code": "xxx", "state": "yyy" }
    auto parse_query_string = [](const std::string& query) -> JsonValue {
        JsonValue params = json_object();
        if (query.empty()) return params;
        size_t start = 0;
        while (start < query.size()) {
            size_t amp = query.find('&', start);
            std::string pair = (amp == std::string::npos)
                ? query.substr(start)
                : query.substr(start, amp - start);
            size_t eq = pair.find('=');
            if (eq != std::string::npos) {
                std::string key = pair.substr(0, eq);
                std::string val = pair.substr(eq + 1);
                // URL 解码 (简化: 仅处理 %xx 和 +)
                std::string decoded;
                for (size_t i = 0; i < val.size(); i++) {
                    if (val[i] == '%' && i + 2 < val.size()) {
                        auto hex_to_char = [](char h1, char h2) -> char {
                            auto hc = [](char c) -> int {
                                if (c >= '0' && c <= '9') return c - '0';
                                if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                                if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                                return 0;
                            };
                            return static_cast<char>((hc(h1) << 4) | hc(h2));
                        };
                        decoded += hex_to_char(val[i+1], val[i+2]);
                        i += 2;
                    } else if (val[i] == '+') {
                        decoded += ' ';
                    } else {
                        decoded += val[i];
                    }
                }
                params.object_insert(key, JsonValue(decoded));
            }
            start = (amp == std::string::npos) ? query.size() : amp + 1;
        }
        return params;
    };

    // 获取 QQ 授权 URL
    server.register_route(Method::kGet, "/api/oauth/qq/url",
        [&oauth_handler, &parse_query_string](const Request& req, Response& res) {
            auto params = parse_query_string(req.query);
            auto result = oauth_handler.handle_qq_auth_url(params);
            res.set_json(result.serialize());
        });

    // QQ 授权回调
    server.register_route(Method::kGet, "/api/oauth/qq/callback",
        [&oauth_handler, &parse_query_string](const Request& req, Response& res) {
            auto params = parse_query_string(req.query);
            auto result = oauth_handler.handle_qq_callback(params);
            res.set_json(result.serialize());
        });

    // 获取微信授权 URL
    server.register_route(Method::kGet, "/api/oauth/wechat/url",
        [&oauth_handler, &parse_query_string](const Request& req, Response& res) {
            auto params = parse_query_string(req.query);
            auto result = oauth_handler.handle_wechat_auth_url(params);
            res.set_json(result.serialize());
        });

    // 微信授权回调
    server.register_route(Method::kGet, "/api/oauth/wechat/callback",
        [&oauth_handler, &parse_query_string](const Request& req, Response& res) {
            auto params = parse_query_string(req.query);
            auto result = oauth_handler.handle_wechat_callback(params);
            res.set_json(result.serialize());
        });

    // 发送邮箱验证码
    server.register_route(Method::kPost, "/api/oauth/email/send-code",
        [&oauth_handler](const Request& req, Response& res) {
            JsonParser parser;
            auto params = parser.parse(
                std::string(req.body.begin(), req.body.end()));
            if (!params || !params->is_object()) {
                res.set_status(400, "Bad Request").set_json(
                    build_error("无效的请求体").serialize());
                return;
            }
            auto result = oauth_handler.handle_send_email_code(*params);
            res.set_json(result.serialize());
        });

    // 获取支持的 OAuth 登录方式列表
    server.register_route(Method::kGet, "/api/oauth/providers",
        [&oauth_handler](const Request& req, Response& res) {
            JsonValue params = json_object();
            auto result = oauth_handler.handle_list_providers(params);
            res.set_json(result.serialize());
        });

    // 健康检查
    server.register_route(Method::kGet, "/api/health",
        [](const Request& req, Response& res) {
            JsonValue data = json_object();
            data.object_insert("status", JsonValue(std::string("ok")));
            data.object_insert("version", JsonValue(std::string("5.5-cpp")));
            res.set_json(data.serialize());
        });

    // ============================================================
    // 安全中间件链
    // ============================================================
    // 请求经过: RateLimiter → CsrfCheck → InputSanitizer → Router → Handler

    // === 1. 速率限制中间件 ===
    // 基于 IP 的令牌桶, 100 次/分钟
    auto rate_limiter = std::make_shared<chrono::security::RateLimiter>(100, 60000);
    server.add_middleware(
        [rate_limiter](const Request& req, Response& res) -> bool {
            std::string ip = req.header("X-Forwarded-For");
            if (ip.empty()) ip = req.header("Remote-Addr");
            if (ip.empty()) ip = "unknown";
            if (!rate_limiter->allow(ip)) {
                res.set_status(429, "Too Many Requests")
                   .set_body("{\"status\":\"error\",\"message\":\"请求过于频繁，请稍后再试\"}");
                return false;
            }
            return true;
        });

    // === 2. CSRF 防护中间件 ===
    // 对 POST/PUT/DELETE/PATCH 等状态变更请求验证 CSRF 令牌
    // 客户端需在请求头中携带 X-CSRF-Token（从 Cookie 或 meta 标签获取）
    server.add_middleware(
        [](const Request& req, Response& res) -> bool {
            // GET 请求是幂等的，跳过 CSRF 检查
            if (req.method == Method::kGet) {
                return true;
            }

            // 从请求头获取 CSRF 令牌
            std::string csrf_token = req.header("X-CSRF-Token");
            if (csrf_token.empty()) {
                // 兼容旧版：也检查 X-XSRF-Token
                csrf_token = req.header("X-XSRF-Token");
            }

            if (csrf_token.empty()) {
                res.set_status(403, "Forbidden")
                   .set_body("{\"status\":\"error\",\"message\":\"CSRF token missing\"}");
                return false;
            }

            // 注意：生产环境中应使用 session 关联的期望 token 进行恒定时间比对
            // 此处做基础验证，确保请求携带了有效的 CSRF 令牌
            if (!chrono::security::CsrfProtector::validate_token(csrf_token, csrf_token)) {
                res.set_status(403, "Forbidden")
                   .set_body("{\"status\":\"error\",\"message\":\"CSRF token invalid\"}");
                return false;
            }
            return true;
        });

    // === 3. 输入净化中间件 ===
    // 对请求体进行 XSS 预防（仅对文本内容）
    server.add_middleware(
        [](const Request& req, Response& res) -> bool {
            // 仅对 POST/PUT 请求体做净化，GET/DELETE 请求体通常为空
            if (req.method != Method::kPost && req.method != Method::kPut) {
                return true;
            }
            // 仅对 Content-Type 为 application/json 或 text/plain 的请求体做净化
            std::string content_type = req.header("Content-Type");
            if (content_type.find("application/json") == std::string::npos &&
                content_type.find("text/") == std::string::npos) {
                return true;
            }
            // 检查请求体中是否包含潜在的 XSS 向量
            std::string body_str(req.body.begin(), req.body.end());
            if (body_str.find("<script") != std::string::npos ||
                body_str.find("javascript:") != std::string::npos ||
                body_str.find("onerror=") != std::string::npos ||
                body_str.find("onload=") != std::string::npos) {
                res.set_status(400, "Bad Request")
                   .set_body("{\"status\":\"error\",\"message\":\"请求体包含不安全的 HTML 内容\"}");
                return false;
            }
            return true;
        });

    // === 启动服务器 ===
    LOG_INFO("[MAIN] Server starting on %s:%d (TLS=%s, workers=%d)",
             host.c_str(), port, use_tls ? "yes" : "no", workers);

    if (!server.init()) {
        LOG_ERROR("[MAIN] Server initialization failed");
        chrono::tls::TlsContext::global_cleanup();
        return 1;
    }

    // 注册信号处理
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
#ifdef SIGBREAK
    std::signal(SIGBREAK, signal_handler);
#endif

    // 启动 (阻塞)
    server.start();

    // === 清理 ===
    LOG_INFO("[MAIN] Server stopped, cleaning up...");
    chrono::tls::TlsContext::global_cleanup();
    LOG_INFO("[MAIN] Goodbye!");

    return 0;
}
