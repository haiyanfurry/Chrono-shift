#ifndef CHRONO_WEBVIEW_MANAGER_H
#define CHRONO_WEBVIEW_MANAGER_H

#include <stdbool.h>

/* ============================================================
 * WebView2 管理器
 * 负责窗口创建和 WebView2 控件管理
 * ============================================================ */

typedef struct {
    void* hwnd;             /* 主窗口句柄 */
    void* webview;          /* WebView2 控制器 */
    int width;              /* 窗口宽度 */
    int height;             /* 窗口高度 */
    char title[256];        /* 窗口标题 */
} WebViewContext;

/* --- API --- */
int  webview_init(WebViewContext* ctx);
int  webview_create_window(WebViewContext* ctx, int width, int height, const char* title);
int  webview_load_html(WebViewContext* ctx, const char* html_path);
int  webview_navigate(WebViewContext* ctx, const char* url);
int  webview_execute_script(WebViewContext* ctx, const char* script);
void webview_destroy(WebViewContext* ctx);
bool webview_process_messages(WebViewContext* ctx);

#endif /* CHRONO_WEBVIEW_MANAGER_H */
