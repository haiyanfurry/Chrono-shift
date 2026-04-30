/**
 * Chrono-shift WebView2 管理器 (骨架)
 * 语言标准: C99
 * 
 * 使用 Windows WebView2 Runtime
 * Windows 10/11 内置
 */

#include "webview_manager.h"
#include "client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

/* 窗口过程 */
static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_SIZE:
            /* Phase 2 调整 WebView2 大小 */
            return 0;
    }
    return DefWindowProc(hwnd, msg, wparam, lparam);
}

int webview_init(WebViewContext* ctx)
{
    memset(ctx, 0, sizeof(WebViewContext));
    LOG_INFO("WebView2 模块初始化");
    return 0;
}

int webview_create_window(WebViewContext* ctx, int width, int height, const char* title)
{
    /* 注册窗口类 */
    HINSTANCE hInstance = GetModuleHandle(NULL);
    
    WNDCLASS wc = {0};
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = "ChronoShiftWindow";

    if (!RegisterClass(&wc)) {
        LOG_ERROR("窗口类注册失败");
        return -1;
    }

    /* 创建窗口 */
    RECT rect = {0, 0, width, height};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hwnd = CreateWindow(
        "ChronoShiftWindow", title,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        NULL, NULL, hInstance, NULL
    );

    if (!hwnd) {
        LOG_ERROR("窗口创建失败");
        return -1;
    }

    ctx->hwnd = (void*)hwnd;
    ctx->width = width;
    ctx->height = height;
    strncpy(ctx->title, title, sizeof(ctx->title) - 1);

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    LOG_INFO("主窗口已创建: %dx%d", width, height);
    return 0;
}

int webview_load_html(WebViewContext* ctx, const char* html_path)
{
    (void)ctx;
    LOG_INFO("加载前端页面: %s", html_path);
    /* Phase 2 使用 WebView2 SDK 加载页面 */
    return 0;
}

int webview_navigate(WebViewContext* ctx, const char* url)
{
    (void)ctx;
    (void)url;
    LOG_DEBUG("导航到: %s", url);
    /* Phase 2 实现 */
    return 0;
}

int webview_execute_script(WebViewContext* ctx, const char* script)
{
    (void)ctx;
    (void)script;
    LOG_DEBUG("执行 JS: %s", script);
    /* Phase 2 实现 */
    return 0;
}

void webview_destroy(WebViewContext* ctx)
{
    if (ctx && ctx->hwnd) {
        DestroyWindow((HWND)ctx->hwnd);
        ctx->hwnd = NULL;
    }
    LOG_INFO("窗口已销毁");
}

bool webview_process_messages(WebViewContext* ctx)
{
    (void)ctx;
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            return false;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return true;
}
