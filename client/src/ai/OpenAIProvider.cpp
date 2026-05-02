/**
 * Chrono-shift OpenAI Provider 实现
 * C++17
 *
 * 使用 WinHTTP 发送请求到 OpenAI 兼容 API
 */
#include "OpenAIProvider.h"

#include <sstream>
#include <algorithm>
#include <cctype>
#include <windows.h>
#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")

namespace chrono {
namespace client {
namespace ai {

// 工厂函数实现
std::unique_ptr<AIProvider> CreateOpenAIProvider(const AIConfig& config) {
    return std::make_unique<OpenAIProvider>(config);
}

OpenAIProvider::OpenAIProvider(const AIConfig& config) {
    set_config(config);
}

void OpenAIProvider::set_config(const AIConfig& config) {
    config_ = config;
    api_endpoint_ = config.api_endpoint;
    api_key_ = config.api_key;
    model_ = config.model_name;
    max_tokens_ = config.max_tokens;
    temperature_ = config.temperature;
}

bool OpenAIProvider::is_available() const {
    return !api_endpoint_.empty() && !api_key_.empty();
}

bool OpenAIProvider::test_connection() {
    if (!is_available()) return false;

    // 发送一个简单的请求测试连接
    std::vector<ChatMessage> test_msg = {
        {"user", "ping"}
    };
    try {
        auto result = chat(test_msg);
        return !result.empty();
    } catch (...) {
        return false;
    }
}

std::string OpenAIProvider::chat(
    const std::vector<ChatMessage>& messages,
    std::function<void(const std::string&)> callback) {

    auto body = build_chat_request(messages);
    auto response = http_post(body);
    auto result = parse_chat_response(response);

    if (callback) {
        callback(result);
    }

    return result;
}

std::string OpenAIProvider::generate(
    const std::string& prompt,
    const std::string& params) {

    std::vector<ChatMessage> messages = {
        {"user", prompt}
    };
    return chat(messages);
}

std::string OpenAIProvider::build_chat_request(
    const std::vector<ChatMessage>& messages) {

    std::ostringstream oss;
    oss << "{";
    oss << "\"model\":\"" << model_ << "\",";
    oss << "\"messages\":[";
    for (size_t i = 0; i < messages.size(); i++) {
        if (i > 0) oss << ",";
        oss << "{";
        oss << "\"role\":\"" << messages[i].role << "\",";
        // 转义 content 中的特殊字符
        std::string escaped = messages[i].content;
        auto escape_pos = std::string::npos;
        // 简单转义双引号和反斜杠
        std::string result;
        for (char c : escaped) {
            if (c == '"') result += "\\\"";
            else if (c == '\\') result += "\\\\";
            else if (c == '\n') result += "\\n";
            else if (c == '\r') result += "\\r";
            else if (c == '\t') result += "\\t";
            else result += c;
        }
        oss << "\"content\":\"" << result << "\"";
        oss << "}";
    }
    oss << "],";
    oss << "\"max_tokens\":" << max_tokens_ << ",";
    oss << "\"temperature\":" << temperature_;
    oss << "}";
    return oss.str();
}

std::string OpenAIProvider::parse_chat_response(const std::string& body) {
    // 简化 JSON 解析 - 提取 "content" 字段
    auto content_key = "\"content\":\"";
    auto pos = body.find(content_key);
    if (pos == std::string::npos) {
        // 尝试查找 error 消息
        auto err_pos = body.find("\"error\"");
        if (err_pos != std::string::npos) {
            return "[API Error] " + body.substr(err_pos, 200);
        }
        return "";
    }

    pos += strlen(content_key);
    std::string result;
    bool escape = false;
    for (; pos < body.size(); pos++) {
        if (escape) {
            if (body[pos] == 'n') result += '\n';
            else if (body[pos] == 'r') result += '\r';
            else if (body[pos] == 't') result += '\t';
            else if (body[pos] == '"') result += '"';
            else if (body[pos] == '\\') result += '\\';
            else result += body[pos];
            escape = false;
        } else if (body[pos] == '\\') {
            escape = true;
        } else if (body[pos] == '"') {
            break;
        } else {
            result += body[pos];
        }
    }
    return result;
}

std::string OpenAIProvider::http_post(const std::string& body) {
    // 使用 WinHTTP 发送请求
    HINTERNET hSession = nullptr;
    HINTERNET hConnect = nullptr;
    HINTERNET hRequest = nullptr;
    std::string result;

    auto parse_url = [](const std::string& url,
                         std::string& host,
                         std::string& path,
                         INTERNET_PORT& port,
                         bool& is_secure) {
        // 解析 URL: https://api.openai.com/v1/chat/completions
        is_secure = url.find("https://") == 0;
        std::string rest = is_secure ? url.substr(8) : url;
        if (!is_secure && url.find("http://") == 0) {
            rest = url.substr(7);
        }

        auto slash_pos = rest.find('/');
        auto colon_pos = rest.find(':');

        if (colon_pos != std::string::npos && (colon_pos < slash_pos || slash_pos == std::string::npos)) {
            host = rest.substr(0, colon_pos);
            auto port_str = slash_pos != std::string::npos
                          ? rest.substr(colon_pos + 1, slash_pos - colon_pos - 1)
                          : rest.substr(colon_pos + 1);
            port = static_cast<INTERNET_PORT>(std::stoi(port_str));
        } else {
            host = slash_pos != std::string::npos ? rest.substr(0, slash_pos) : rest;
            port = is_secure ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;
        }

        path = slash_pos != std::string::npos ? rest.substr(slash_pos) : "/";
    };

    std::string host, path;
    INTERNET_PORT port;
    bool is_secure;
    parse_url(api_endpoint_, host, path, port, is_secure);

    hSession = WinHttpOpen(L"Chrono-shift AI/1.0",
                           WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                           nullptr, nullptr, 0);
    if (!hSession) return "";

    std::wstring whost(host.begin(), host.end());
    hConnect = WinHttpConnect(hSession, whost.c_str(), port, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return "";
    }

    std::wstring wpath(path.begin(), path.end());
    hRequest = WinHttpOpenRequest(hConnect, L"POST", wpath.c_str(),
                                  nullptr, nullptr, nullptr,
                                  is_secure ? WINHTTP_FLAG_SECURE : 0);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }

    // 设置 headers
    std::wstring auth_header = L"Authorization: Bearer " + std::wstring(api_key_.begin(), api_key_.end());
    std::wstring content_type = L"Content-Type: application/json";
    WinHttpAddRequestHeaders(hRequest, auth_header.c_str(), -1, WINHTTP_ADDREQ_FLAG_ADD);
    WinHttpAddRequestHeaders(hRequest, content_type.c_str(), -1, WINHTTP_ADDREQ_FLAG_ADD);

    // 发送请求
    if (!WinHttpSendRequest(hRequest, nullptr, 0,
                            const_cast<char*>(body.data()),
                            static_cast<DWORD>(body.size()),
                            static_cast<DWORD>(body.size()), 0)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }

    if (!WinHttpReceiveResponse(hRequest, nullptr)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }

    // 读取响应
    DWORD bytes_available = 0;
    std::vector<char> buffer;
    while (WinHttpQueryDataAvailable(hRequest, &bytes_available) && bytes_available > 0) {
        buffer.resize(buffer.size() + bytes_available + 1);
        DWORD bytes_read = 0;
        WinHttpReadData(hRequest, buffer.data() + buffer.size() - bytes_available - 1,
                        bytes_available, &bytes_read);
        buffer[buffer.size() - bytes_available - 1 + bytes_read] = '\0';
    }

    result = buffer.data();

    // 清理
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return result;
}

} // namespace ai
} // namespace client
} // namespace chrono
