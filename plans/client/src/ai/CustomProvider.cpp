/**
 * Chrono-shift 自定义 AI Provider 实现
 * C++17
 */
#include "CustomProvider.h"

#include <sstream>
#include <windows.h>
#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")

namespace chrono {
namespace client {
namespace ai {

// 工厂函数实现
std::unique_ptr<AIProvider> CreateCustomProvider(const AIConfig& config) {
    return std::make_unique<CustomProvider>(config);
}

CustomProvider::CustomProvider(const AIConfig& config) {
    set_config(config);
}

void CustomProvider::set_config(const AIConfig& config) {
    config_ = config;
    api_endpoint_ = config.api_endpoint;
    api_key_ = config.api_key;
    model_ = config.model_name;
    max_tokens_ = config.max_tokens;
    temperature_ = config.temperature;
}

bool CustomProvider::is_available() const {
    return !api_endpoint_.empty();
}

bool CustomProvider::test_connection() {
    if (!is_available()) return false;
    try {
        auto result = http_post(api_endpoint_, "{\"test\":true}", "");
        return !result.empty();
    } catch (...) {
        return false;
    }
}

std::string CustomProvider::chat(
    const std::vector<ChatMessage>& messages,
    std::function<void(const std::string&)> callback) {

    // 构建简单的 JSON 请求体
    std::ostringstream oss;
    oss << "{\"messages\":[";
    for (size_t i = 0; i < messages.size(); i++) {
        if (i > 0) oss << ",";
        oss << "{\"role\":\"" << messages[i].role
            << "\",\"content\":\"" << messages[i].content << "\"}";
    }
    oss << "],\"model\":\"" << model_ << "\"}";

    std::string auth_header;
    if (!api_key_.empty()) {
        auth_header = "Authorization: Bearer " + api_key_;
    }

    auto response = http_post(api_endpoint_, oss.str(), auth_header);

    if (callback) {
        callback(response);
    }

    return response;
}

std::string CustomProvider::generate(
    const std::string& prompt,
    const std::string& params) {

    std::vector<ChatMessage> messages = {
        {"user", prompt}
    };
    return chat(messages);
}

std::string CustomProvider::http_post(
    const std::string& endpoint,
    const std::string& body,
    const std::string& auth_header) {

    HINTERNET hSession = nullptr;
    HINTERNET hConnect = nullptr;
    HINTERNET hRequest = nullptr;
    std::string result;

    auto parse_url = [](const std::string& url,
                         std::string& host,
                         std::string& path,
                         INTERNET_PORT& port,
                         bool& is_secure) {
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
    parse_url(endpoint, host, path, port, is_secure);

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

    WinHttpAddRequestHeaders(hRequest, L"Content-Type: application/json", -1, WINHTTP_ADDREQ_FLAG_ADD);

    if (!auth_header.empty()) {
        std::wstring wauth(auth_header.begin(), auth_header.end());
        WinHttpAddRequestHeaders(hRequest, wauth.c_str(), -1, WINHTTP_ADDREQ_FLAG_ADD);
    }

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

    DWORD bytes_available = 0;
    std::vector<char> buffer;
    while (WinHttpQueryDataAvailable(hRequest, &bytes_available) && bytes_available > 0) {
        size_t old_size = buffer.size();
        buffer.resize(old_size + bytes_available + 1);
        DWORD bytes_read = 0;
        WinHttpReadData(hRequest, buffer.data() + old_size, bytes_available, &bytes_read);
        buffer[old_size + bytes_read] = '\0';
    }

    result = buffer.data();

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return result;
}

} // namespace ai
} // namespace client
} // namespace chrono
