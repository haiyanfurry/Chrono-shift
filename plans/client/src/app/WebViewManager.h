/**
 * Chrono-shift 客户端 WebView2 管理器
 * C++17 重构版
 *
 * 使用 Windows WebView2 Runtime
 * 负责窗口创建、WebView2 控件管理、JS 互操作
 */
#ifndef CHRONO_CLIENT_WEBVIEW_MANAGER_H
#define CHRONO_CLIENT_WEBVIEW_MANAGER_H

#include <cstdint>
#include <string>

#include <windows.h>

namespace chrono {
namespace client {
namespace app {

/**
 * WebView2 窗口管理器
 *
 * 封装 Win32 窗口创建与 WebView2 控件生命周期
 * (WebView2 SDK 集成为 Phase 2)
 *
 * 使用示例:
 * @code
 *   WebViewManager wm;
 *   wm.create_window(1280, 720, "Chrono-shift");
 *   wm.load_html("client/ui/index.html");
 *   while (wm.process_messages()) { ... }
 * @endcode
 */
class WebViewManager {
public:
    /** 默认窗口类名 */
    static constexpr const char* kWindowClass = "ChronoShiftWindow";

    WebViewManager();
    ~WebViewManager();

    /* 禁止拷贝，允许移动 */
    WebViewManager(const WebViewManager&) = delete;
    WebViewManager& operator=(const WebViewManager&) = delete;
    WebViewManager(WebViewManager&&) = default;
    WebViewManager& operator=(WebViewManager&&) = default;

    // ============================================================
    // 窗口管理
    // ============================================================

    /**
     * 创建主窗口
     * @param width  窗口宽度
     * @param height 窗口高度
     * @param title  窗口标题
     * @return true=创建成功
     */
    bool create_window(int width, int height, const std::string& title);

    /**
     * 销毁窗口
     */
    void destroy();

    /**
     * 处理 Windows 消息循环
     * @return false 当收到 WM_QUIT 时
     */
    bool process_messages();

    /**
     * 获取窗口句柄
     */
    HWND get_hwnd() const;

    /**
     * 获取窗口宽度
     */
    int get_width() const;

    /**
     * 获取窗口高度
     */
    int get_height() const;

    // ============================================================
    // WebView2 操作 (Phase 2 实现)
    // ============================================================

    /**
     * 加载本地 HTML 文件
     * @param html_path HTML 文件路径
     * @return 0=成功
     */
    int load_html(const std::string& html_path);

    /**
     * 导航到 URL
     * @param url 目标 URL
     * @return 0=成功
     */
    int navigate(const std::string& url);

    /**
     * 执行 JavaScript
     * @param script JS 代码
     * @return 0=成功
     */
    int execute_script(const std::string& script);

    /**
     * 获取 WebView2 控制器指针 (void* 避免硬依赖)
     */
    void* get_webview() const;

private:
    /** 窗口过程 (静态) */
    static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg,
                                     WPARAM wparam, LPARAM lparam);

    /** Win32 窗口句柄 */
    HWND hwnd_ = nullptr;

    /** WebView2 控制器 (void* 避免依赖 WebView2 SDK 头) */
    void* webview_ = nullptr;

    /** 窗口尺寸 */
    int width_ = 0;
    int height_ = 0;

    /** 窗口标题 */
    std::string title_;

    /** 窗口类是否已注册 (静态) */
    static bool s_class_registered_;
};

} // namespace app
} // namespace client
} // namespace chrono

#endif // CHRONO_CLIENT_WEBVIEW_MANAGER_H
