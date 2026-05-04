/**
 * GlueTypes.cpp — 共享类型序列化实现
 */
#include "glue/GlueTypes.h"
#include <sstream>

namespace chrono { namespace glue {

std::string Envelope::to_json() const
{
    std::ostringstream oss;
    oss << "{\"id\":\"" << id
        << "\",\"from\":\"" << from_uid
        << "\",\"to\":\"" << to_uid
        << "\",\"text\":\"" << text
        << "\",\"ts\":" << timestamp
        << ",\"via\":" << (int)via << "}";
    return oss.str();
}

Envelope Envelope::from_json(const std::string& json)
{
    Envelope e;
    // 简易解析 (生产用应使用 json_parser)
    auto extract = [&](const char* key) -> std::string {
        std::string k = std::string("\"") + key + "\":\"";
        auto pos = json.find(k);
        if (pos == std::string::npos) return "";
        pos += k.size();
        auto end = json.find('"', pos);
        return json.substr(pos, end - pos);
    };
    e.id = extract("id");
    e.from_uid = extract("from");
    e.to_uid = extract("to");
    e.text = extract("text");
    return e;
}

std::vector<char16_t> SafeString::to_utf16() const
{
    std::vector<char16_t> result;
    for (size_t i = 0; i < utf8.size(); ) {
        unsigned char c = (unsigned char)utf8[i];
        if (c < 0x80) {
            result.push_back(c);
            i += 1;
        } else if (c < 0xE0) {
            if (i + 1 < utf8.size())
                result.push_back(((c & 0x1F) << 6) | (utf8[i+1] & 0x3F));
            i += 2;
        } else if (c < 0xF0) {
            if (i + 2 < utf8.size())
                result.push_back(((c & 0x0F) << 12) | ((utf8[i+1] & 0x3F) << 6) | (utf8[i+2] & 0x3F));
            i += 3;
        } else {
            // 代理对 (surrogate pair)
            if (i + 3 < utf8.size()) {
                uint32_t cp = ((c & 0x07) << 18) | ((utf8[i+1] & 0x3F) << 12)
                            | ((utf8[i+2] & 0x3F) << 6) | (utf8[i+3] & 0x3F);
                cp -= 0x10000;
                result.push_back(0xD800 | (cp >> 10));
                result.push_back(0xDC00 | (cp & 0x3FF));
            }
            i += 4;
        }
    }
    return result;
}

SafeString SafeString::from_utf16(const char16_t* data, size_t len)
{
    SafeString s;
    for (size_t i = 0; i < len; i++) {
        uint32_t cp = data[i];
        if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < len) {
            // 高代理 → 解码代理对
            uint32_t lo = data[++i];
            if (lo >= 0xDC00 && lo <= 0xDFFF) {
                cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
            }
        }
        if (cp < 0x80) {
            s.utf8 += (char)cp;
        } else if (cp < 0x800) {
            s.utf8 += (char)(0xC0 | (cp >> 6));
            s.utf8 += (char)(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            s.utf8 += (char)(0xE0 | (cp >> 12));
            s.utf8 += (char)(0x80 | ((cp >> 6) & 0x3F));
            s.utf8 += (char)(0x80 | (cp & 0x3F));
        } else {
            s.utf8 += (char)(0xF0 | (cp >> 18));
            s.utf8 += (char)(0x80 | ((cp >> 12) & 0x3F));
            s.utf8 += (char)(0x80 | ((cp >> 6) & 0x3F));
            s.utf8 += (char)(0x80 | (cp & 0x3F));
        }
    }
    return s;
}

SafeString SafeString::from_utf8(const std::string& s_in)
{
    SafeString s;
    s.utf8 = s_in;  // 生产环境应调用 Rust validate_utf8
    return s;
}

} } // namespace
