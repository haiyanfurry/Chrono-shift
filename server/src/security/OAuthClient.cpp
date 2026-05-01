/**
 * Chrono-shift C++ OAuth HTTP 客户端实现
 * 使用平台 socket 调用 QQ/微信 OAuth2.0 API
 * C++17 重构版 (P9.2)
 */
#include "OAuthClient.h"
#include "../util/Logger.h"
#include <cstring>
#include <sstream>
#include <iomanip>
#include <regex>

// 平台 socket
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#endif

namespace chrono {
namespace security {

// ============================================================
// 工具函数
// ============================================================

/**
 * URL 编码 (Percent Encoding)
 */
static std::string url_encode(const std::string& value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (unsigned char c : value) {
        // 保留字母数字和部分符号
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else {
            escaped << '%' << std::setw(2) << static_cast<int>(c);
        }
    }
    return escaped.str();
}

/**
 * 从完整 URL 中提取主机名和路径
 * 如: https://graph.qq.com/oauth2.0/token?code=xxx
 * -> host=graph.qq.com, path=/oauth2.0/token?code=xxx, use_ssl=true
 */
static bool parse_url(const std::string& url,
                      std::string& host,
                      std::string& path,
                      int& port,
                      bool& use_ssl) {
    std::regex re(R"(^(https?://)?([^:/?#]+)(?::(\d+))?([^?#]*)(\?[^#]*)?)");
    std::smatch m;
    if (!std::regex_match(url, m, re)) {
        return false;
    }

    use_ssl = (m[1].str() == "https://");
    host = m[2].str();
    path = m[4].str() + m[5].str();
    if (path.empty()) path = "/";

    if (m[3].matched) {
        port = std::stoi(m[3].str());
    } else {
        port = use_ssl ? 443 : 80;
    }
    return true;
}

/**
 * 执行 HTTP GET 请求 (非 TLS)
 * @param host 主机名
 * @param port 端口
 * @param path 请求路径
 * @param[out] response_body 响应体
 * @return true 表示成功
 */
static bool http_get_plain(const std::string& host, int port,
                           const std::string& path,
                           std::string& response_body) {
#ifdef _WIN32
    // 初始化 Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        LOG_ERROR("[OAuthClient] WSAStartup failed");
        return false;
    }
#endif

    // DNS 解析
    struct addrinfo hints, *res = nullptr;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    std::string port_str = std::to_string(port);
    int ret = getaddrinfo(host.c_str(), port_str.c_str(), &hints, &res);
    if (ret != 0 || !res) {
        LOG_ERROR("[OAuthClient] DNS resolution failed for %s: %d", host.c_str(), ret);
#ifdef _WIN32
        WSACleanup();
#endif
        return false;
    }

    // 创建 socket
    SOCKET sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock == INVALID_SOCKET) {
        LOG_ERROR("[OAuthClient] socket() failed");
        freeaddrinfo(res);
#ifdef _WIN32
        WSACleanup();
#endif
        return false;
    }

#ifdef _WIN32
    // 设置超时
    int timeout = 10000; // 10秒
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));
#endif

    // 连接
    if (connect(sock, res->ai_addr, (int)res->ai_addrlen) < 0) {
        LOG_ERROR("[OAuthClient] connect() failed to %s:%d", host.c_str(), port);
#ifdef _WIN32
        closesocket(sock);
#else
        ::close(sock);
#endif
        freeaddrinfo(res);
#ifdef _WIN32
        WSACleanup();
#endif
        return false;
    }
    freeaddrinfo(res);

    // 构建 HTTP GET 请求
    std::ostringstream request;
    request << "GET " << path << " HTTP/1.1\r\n"
            << "Host: " << host << "\r\n"
            << "Connection: close\r\n"
            << "User-Agent: Chrono-shift-OAuth/1.0\r\n"
            << "Accept: */*\r\n"
            << "\r\n";

    std::string req_str = request.str();
    if (send(sock, req_str.c_str(), (int)req_str.size(), 0) < 0) {
        LOG_ERROR("[OAuthClient] send() failed");
#ifdef _WIN32
        closesocket(sock);
        WSACleanup();
#else
        ::close(sock);
#endif
        return false;
    }

    // 读取响应
    std::string raw_response;
    char buf[4096];
    int bytes_read;
    while ((bytes_read = (int)recv(sock, buf, sizeof(buf) - 1, 0)) > 0) {
        buf[bytes_read] = '\0';
        raw_response += buf;
    }

#ifdef _WIN32
    closesocket(sock);
    WSACleanup();
#else
    ::close(sock);
#endif

    // 解析 HTTP 响应，提取 body
    // 查找空行分隔头/体
    size_t header_end = raw_response.find("\r\n\r\n");
    if (header_end == std::string::npos) {
        LOG_ERROR("[OAuthClient] Invalid HTTP response from %s", host.c_str());
        return false;
    }

    // 检查 HTTP 状态码
    std::string status_line = raw_response.substr(0, raw_response.find("\r\n"));
    if (status_line.find("200") == std::string::npos &&
        status_line.find("302") == std::string::npos) {
        LOG_ERROR("[OAuthClient] HTTP error from %s: %s", host.c_str(), status_line.c_str());
        // 仍然尝试解析 body
    }

    response_body = raw_response.substr(header_end + 4);
    return !response_body.empty();
}

// ============================================================
// QQClient 实现
// ============================================================

QQClient::QQClient(OAuthClientConfig config)
    : config_(std::move(config)) {
}

std::string QQClient::build_auth_url(const std::string& state) const {
    std::ostringstream url;
    url << "https://graph.qq.com/oauth2.0/authorize?"
        << "response_type=code"
        << "&client_id=" << url_encode(config_.app_id)
        << "&redirect_uri=" << url_encode(config_.redirect_uri)
        << "&state=" << url_encode(state)
        << "&scope=get_user_info";
    return url.str();
}

bool QQClient::exchange_code(const std::string& code,
                              std::string& access_token,
                              std::string& open_id) {
    // Step 1: code -> access_token
    std::ostringstream token_url;
    token_url << "https://graph.qq.com/oauth2.0/token?"
              << "grant_type=authorization_code"
              << "&client_id=" << url_encode(config_.app_id)
              << "&client_secret=" << url_encode(config_.app_key)
              << "&code=" << url_encode(code)
              << "&redirect_uri=" << url_encode(config_.redirect_uri)
              << "&fmt=json";  // 要求 JSON 格式响应

    std::string token_response;
    if (!http_get(token_url.str(), token_response)) {
        LOG_ERROR("[QQClient] Failed to exchange code for token");
        return false;
    }

    // 解析 JSON 响应: {"access_token":"xxx","expires_in":7776000}
    // 简单解析 (不使用完整 JSON 解析器, 避免循环依赖)
    auto find_json_str = [](const std::string& json, const std::string& key) -> std::string {
        std::string search = "\"" + key + "\":\"";
        size_t pos = json.find(search);
        if (pos == std::string::npos) return "";
        pos += search.size();
        size_t end = json.find("\"", pos);
        if (end == std::string::npos) return "";
        return json.substr(pos, end - pos);
    };

    access_token = find_json_str(token_response, "access_token");
    if (access_token.empty()) {
        LOG_ERROR("[QQClient] No access_token in response: %s", token_response.c_str());
        return false;
    }

    // Step 2: access_token -> open_id
    // GET https://graph.qq.com/oauth2.0/me?access_token=TOKEN
    // 响应: callback({"client_id":"xxx","openid":"xxx"});
    std::ostringstream me_url;
    me_url << "https://graph.qq.com/oauth2.0/me?access_token="
           << url_encode(access_token);

    std::string me_response;
    if (!http_get(me_url.str(), me_response)) {
        LOG_ERROR("[QQClient] Failed to get open_id");
        return false;
    }

    // 提取 JSON 部分: callback({...});
    size_t json_start = me_response.find('{');
    size_t json_end = me_response.rfind('}');
    if (json_start == std::string::npos || json_end == std::string::npos) {
        LOG_ERROR("[QQClient] Invalid open_id response: %s", me_response.c_str());
        return false;
    }
    std::string json_part = me_response.substr(json_start, json_end - json_start + 1);
    open_id = find_json_str(json_part, "openid");
    if (open_id.empty()) {
        LOG_ERROR("[QQClient] No openid in response: %s", me_response.c_str());
        return false;
    }

    LOG_INFO("[QQClient] Code exchange success: open_id=%s", open_id.c_str());
    return true;
}

bool QQClient::get_user_info(const std::string& access_token,
                              const std::string& open_id,
                              OAuthUserInfo& info) {
    std::ostringstream url;
    url << "https://graph.qq.com/user/get_user_info?"
        << "access_token=" << url_encode(access_token)
        << "&oauth_consumer_key=" << url_encode(config_.app_id)
        << "&openid=" << url_encode(open_id);

    std::string response;
    if (!http_get(url.str(), response)) {
        LOG_ERROR("[QQClient] Failed to get user info");
        return false;
    }

    // 解析 JSON 响应
    auto find_json_str = [](const std::string& json, const std::string& key) -> std::string {
        std::string search = "\"" + key + "\":\"";
        size_t pos = json.find(search);
        if (pos == std::string::npos) {
            // 尝试不带引号的值 (如数字)
            std::string search2 = "\"" + key + "\":";
            pos = json.find(search2);
            if (pos == std::string::npos) return "";
            pos += search2.size();
            size_t end = json.find_first_of(",}", pos);
            if (end == std::string::npos) return "";
            return json.substr(pos, end - pos);
        }
        pos += search.size();
        size_t end = json.find("\"", pos);
        if (end == std::string::npos) return "";
        return json.substr(pos, end - pos);
    };

    info.open_id = open_id;
    info.nickname = find_json_str(response, "nickname");
    info.avatar_url = find_json_str(response, "figureurl_qq_2");
    if (info.avatar_url.empty()) {
        info.avatar_url = find_json_str(response, "figureurl_qq_1");
    }

    LOG_INFO("[QQClient] Got user info: nickname=%s", info.nickname.c_str());
    return true;
}

bool QQClient::verify_token(const std::string& access_token,
                             const std::string& open_id) {
    // QQ 验证 access_token: GET https://graph.qq.com/oauth2.0/me?access_token=TOKEN
    std::ostringstream url;
    url << "https://graph.qq.com/oauth2.0/me?access_token="
        << url_encode(access_token);

    std::string response;
    if (!http_get(url.str(), response)) {
        return false;
    }

    // 检查响应中是否包含对应的 openid
    size_t json_start = response.find('{');
    size_t json_end = response.rfind('}');
    if (json_start == std::string::npos || json_end == std::string::npos) {
        return false;
    }
    std::string json_part = response.substr(json_start, json_end - json_start + 1);
    return json_part.find(open_id) != std::string::npos;
}

bool QQClient::http_get(const std::string& url, std::string& response_body) {
    std::string host, path;
    int port;
    bool use_ssl;

    if (!parse_url(url, host, path, port, use_ssl)) {
        LOG_ERROR("[QQClient] Failed to parse URL: %s", url.c_str());
        return false;
    }

    // 暂不支持 TLS (使用普通 HTTP GET)
    // QQ/微信 OAuth API 使用 HTTPS, 但我们的 socket 实现为简化版
    // 生产环境中应使用 TLS 包装
    return http_get_plain(host, port, path, response_body);
}

// ============================================================
// WechatClient 实现
// ============================================================

WechatClient::WechatClient(OAuthClientConfig config)
    : config_(std::move(config)) {
}

std::string WechatClient::build_auth_url(const std::string& state) const {
    std::ostringstream url;
    url << "https://open.weixin.qq.com/connect/qrconnect?"
        << "appid=" << url_encode(config_.app_id)
        << "&redirect_uri=" << url_encode(config_.redirect_uri)
        << "&response_type=code"
        << "&scope=snsapi_login"
        << "&state=" << url_encode(state)
        << "#wechat_redirect";
    return url.str();
}

bool WechatClient::exchange_code(const std::string& code,
                                  std::string& access_token,
                                  std::string& open_id) {
    std::ostringstream url;
    url << "https://api.weixin.qq.com/sns/oauth2/access_token?"
        << "appid=" << url_encode(config_.app_id)
        << "&secret=" << url_encode(config_.app_key)
        << "&code=" << url_encode(code)
        << "&grant_type=authorization_code";

    std::string response;
    if (!http_get(url.str(), response)) {
        LOG_ERROR("[WechatClient] Failed to exchange code");
        return false;
    }

    // 解析 JSON 响应: {"access_token":"xxx","expires_in":7200,"refresh_token":"xxx","openid":"xxx","scope":"xxx"}
    auto find_json_str = [](const std::string& json, const std::string& key) -> std::string {
        std::string search = "\"" + key + "\":\"";
        size_t pos = json.find(search);
        if (pos == std::string::npos) return "";
        pos += search.size();
        size_t end = json.find("\"", pos);
        if (end == std::string::npos) return "";
        return json.substr(pos, end - pos);
    };

    // 检查是否有错误
    std::string errcode = find_json_str(response, "errcode");
    if (!errcode.empty() && errcode != "0") {
        std::string errmsg = find_json_str(response, "errmsg");
        LOG_ERROR("[WechatClient] API error: %s - %s", errcode.c_str(), errmsg.c_str());
        return false;
    }

    access_token = find_json_str(response, "access_token");
    open_id = find_json_str(response, "openid");

    if (access_token.empty() || open_id.empty()) {
        LOG_ERROR("[WechatClient] Missing token/openid in response: %s", response.c_str());
        return false;
    }

    LOG_INFO("[WechatClient] Code exchange success: open_id=%s", open_id.c_str());
    return true;
}

bool WechatClient::get_user_info(const std::string& access_token,
                                  const std::string& open_id,
                                  OAuthUserInfo& info) {
    std::ostringstream url;
    url << "https://api.weixin.qq.com/sns/userinfo?"
        << "access_token=" << url_encode(access_token)
        << "&openid=" << url_encode(open_id);

    std::string response;
    if (!http_get(url.str(), response)) {
        LOG_ERROR("[WechatClient] Failed to get user info");
        return false;
    }

    auto find_json_str = [](const std::string& json, const std::string& key) -> std::string {
        std::string search = "\"" + key + "\":\"";
        size_t pos = json.find(search);
        if (pos == std::string::npos) return "";
        pos += search.size();
        size_t end = json.find("\"", pos);
        if (end == std::string::npos) return "";
        return json.substr(pos, end - pos);
    };

    std::string errcode = find_json_str(response, "errcode");
    if (!errcode.empty() && errcode != "0") {
        LOG_ERROR("[WechatClient] Userinfo API error: %s", response.c_str());
        return false;
    }

    info.open_id = open_id;
    info.nickname = find_json_str(response, "nickname");
    info.avatar_url = find_json_str(response, "headimgurl");

    LOG_INFO("[WechatClient] Got user info: nickname=%s", info.nickname.c_str());
    return true;
}

bool WechatClient::verify_token(const std::string& access_token,
                                 const std::string& open_id) {
    // 微信验证 access_token: GET https://api.weixin.qq.com/sns/auth?access_token=TOKEN&openid=OPENID
    std::ostringstream url;
    url << "https://api.weixin.qq.com/sns/auth?"
        << "access_token=" << url_encode(access_token)
        << "&openid=" << url_encode(open_id);

    std::string response;
    if (!http_get(url.str(), response)) {
        return false;
    }

    // 成功响应: {"errcode":0,"errmsg":"ok"}
    return response.find("\"errcode\":0") != std::string::npos;
}

bool WechatClient::http_get(const std::string& url, std::string& response_body) {
    std::string host, path;
    int port;
    bool use_ssl;

    if (!parse_url(url, host, path, port, use_ssl)) {
        LOG_ERROR("[WechatClient] Failed to parse URL: %s", url.c_str());
        return false;
    }

    return http_get_plain(host, port, path, response_body);
}

} // namespace security
} // namespace chrono
