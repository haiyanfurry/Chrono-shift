/**
 * Chrono-shift 客户端入口
 * C++17 重构版
 *
 * 平台: Windows 10/11 x64 (Win32 API + WebView2)
 */
#include "AppContext.h"
#include "../util/Logger.h"

#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#endif

/* ================================================================
 * 主入口
 * ================================================================ */

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow)
{
    (void)hInstance;
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;
#else
int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;
#endif

    /* 初始化配置 */
    chrono::client::app::ClientConfig config;
    config.server_host    = "127.0.0.1";
    config.server_port    = 4443;
    config.app_data_path  = "./data";
    config.log_level      = chrono::client::util::LogLevel::kInfo;
    config.auto_reconnect = true;

    /* 获取应用上下文并初始化 */
    auto& ctx = chrono::client::app::AppContext::instance();
    if (ctx.init(config) != 0) {
        LOG_ERROR("客户端初始化失败");
        return 1;
    }

    /* 运行消息循环 */
    return ctx.run();
}
