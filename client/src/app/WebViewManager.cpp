/**
 * Chrono-shift 客户端 WebView2 管理器实现
 * C++17 重构版
 */

#include "WebViewManager.h"

#include "../util/Logger.h"

namespace chrono {
namespace client {
namespace app {

/* 静态成员初始化 */
bool WebViewManager::s_class_registered_ = false;

/* ============================================================
 * 窗口过程 (静态)
 * ============================================================ */

LRESULT CALLBACK WebViewManager::wnd_proc(HWND hwnd, UINT msg,
                                          WPARAM wparam, LPARAM lparam)
{
    switch (msg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        case WM_SIZE:
            /* Phase 2: 调整 WebView2 大小 */
            return 0;
    }

    return DefWindowProc(hwnd, msg, wparam, lparam);
}

/* ============================================================
 * 构造函数 / 析构函数
 * ============================================================ */

WebViewManager::WebViewManager()
    : hwnd_(nullptr)
    , webview_(nullptr)
    , width_(0)
    , height_(0)
{
}

WebViewManager::~WebViewManager()
{
    destroy();
}

/* ============================================================
 * 窗口管理
 * ============================================================ */

bool WebViewManager::create_window(int width, int height, const std::string& title)
{
    width_  = width;
    height_ = height;
    title_  = title;

    HINSTANCE hInstance = GetModuleHandle(nullptr);

    /* 注册窗口类 (仅一次) */
    if (!s_class_registered_) {
        WNDCLASSA wc = {};
        wc.lpfnWndProc   = wnd_proc;
        wc.hInstance     = hInstance;
        wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        wc.lpszClassName = kWindowClass;

        if (!RegisterClassA(&wc)) {
            LOG_ERROR("窗口类注册失败 (error=%lu)", GetLastError());
            return false;
        }
        s_class_registered_ = true;
    }

    /* 计算窗口尺寸 (含标题栏/边框) */
    RECT rect = { 0, 0, width, height };
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    /* 创建窗口 */
    hwnd_ = CreateWindowA(
        kWindowClass,
        title.c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    if (!hwnd_) {
        LOG_ERROR("窗口创建失败 (error=%lu)", GetLastError());
        return false;
    }

    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);

    LOG_INFO("主窗口已创建: %dx%d \"%s\"", width, height, title.c_str());
    return true;
}

void WebViewManager::destroy()
{
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }

    /* 注意: 不注销窗口类，因为可能还有消息需要处理 */

    webview_ = nullptr;
    LOG_INFO("窗口已销毁");
}

bool WebViewManager::process_messages()
{
    MSG msg;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            return false;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return true;
}

HWND WebViewManager::get_hwnd() const
{
    return hwnd_;
}

int WebViewManager::get_width() const
{
    return width_;
}

int WebViewManager::get_height() const
{
    return height_;
}

/* ============================================================
 * WebView2 操作 (Phase 2 实现)
 * ============================================================ */

int WebViewManager::load_html(const std::string& html_path)
{
    LOG_INFO("加载前端页面: %s", html_path.c_str());
    /* Phase 2: 使用 WebView2 SDK 加载本地文件:
     *   wil::com_ptr<ICoreWebView2> webview;
     *   webview->Navigate(html_path.c_str());
     */
    (void)html_path;
    return 0;
}

int WebViewManager::navigate(const std::string& url)
{
    LOG_DEBUG("导航到: %s", url.c_str());
    /* Phase 2: webview->Navigate(url.c_str()); */
    (void)url;
    return 0;
}

int WebViewManager::execute_script(const std::string& script)
{
    LOG_DEBUG("执行 JS: %s", script.c_str());
    /* Phase 2: webview->ExecuteScript(script.c_str(), nullptr); */
    (void)script;
    return 0;
}

void* WebViewManager::get_webview() const
{
    return webview_;
}

} // namespace app
} // namespace client
} // namespace chrono
