#pragma once
/**
 * JavaBridge.h — Java JNI 胶水层桩
 *
 * 为后期 Android / 桌面 GUI (JavaFX/Swing) 预留接口。
 * Java 通过 JNI 调用此层, 避免直接操作 C++ 对象。
 *
 * 关键: 所有字符串使用 SafeString (UTF-8↔UTF-16 安全转换),
 * 防止 Java UTF-16 代理对 (surrogate pairs) 截断。
 */
#include "glue/GlueTypes.h"
#include <string>
#include <vector>
#include <jni.h>

namespace chrono { namespace glue { namespace java {

class JavaBridge {
public:
    static JavaBridge& instance();

    // JNI 初始化 (Java 调用)
    bool jni_init(JavaVM* jvm, JNIEnv* env);

    // === 身份 (Java → C++) ===
    std::string get_uid();           // 返回 SafeString.utf8
    void set_uid(const std::string& uid);
    std::string get_address();

    // === 好友 ===
    std::string friend_list_json();  // JSON 数组, SafeString 编码
    std::string pending_requests_json();
    bool accept_friend(const std::string& uid);
    bool reject_friend(const std::string& uid);

    // === 消息 ===
    bool send_message(const std::string& to, const std::string& text);
    std::string get_messages_json(const std::string& with_uid);

    // === 传输状态 ===
    std::string transport_status_json();

    // === 事件回调 (C++ → Java) ===
    // Java 端实现 onEvent(String type, String json)
    void set_event_listener(jobject listener);

private:
    JavaBridge() = default;
    JavaVM* jvm_ = nullptr;
    jobject listener_ = nullptr;
};

// SafeString UTF-8 ↔ UTF-16 实现
// 确保所有 Java ↔ C++ 字符串转换通过此函数,
// 防止 Java 代理对 (surrogate pairs) 在边界截断
std::string jstring_to_utf8(JNIEnv* env, jstring str);
jstring utf8_to_jstring(JNIEnv* env, const std::string& str);

} } } // namespace chrono::glue::java
