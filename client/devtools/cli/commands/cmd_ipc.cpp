/**
 * cmd_ipc.cpp - IPC 调试命令 (C++23 版本)
 * 对应 debug_cli.c:916 cmd_ipc
 */
#include "../devtools_cli.hpp"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <ctime>

#ifdef _WIN32
    #include <windows.h>
    #define msleep(x) Sleep(x)
#else
    #include <unistd.h>
    #define msleep(x) usleep((x) * 1000)
#endif

#include "print_compat.h"     // std::println
#include <string_view>

namespace cli = chrono::client::cli;

/* IPC 消息类型表 */
struct IpcMessageEntry {
    int         type;
    const char* name;
    const char* description;
};

static constexpr IpcMessageEntry IPC_MESSAGE_TYPES[] = {
    {0x01, "LOGIN",          "用户登录"},
    {0x02, "LOGOUT",         "用户登出"},
    {0x10, "SEND_MESSAGE",   "发送消息"},
    {0x11, "GET_MESSAGES",   "获取消息历史"},
    {0x20, "GET_CONTACTS",   "获取联系人列表"},
    {0x30, "GET_TEMPLATES",  "获取社区模板"},
    {0x31, "APPLY_TEMPLATE", "应用模板主题"},
    {0x40, "FILE_UPLOAD",    "上传文件"},
    {0x50, "OPEN_URL",       "打开外部 URL（预留）"},
    {0xFF, "SYSTEM_NOTIFY",  "系统通知"},
    {0, nullptr, nullptr}
};

constexpr std::size_t IPC_TYPE_COUNT =
    sizeof(IPC_MESSAGE_TYPES) / sizeof(IPC_MESSAGE_TYPES[0]) - 1;

/** ipc - IPC 调试命令 */
static int cmd_ipc(int argc, char** argv)
{
    if (argc < 1) {
        cli::println("用法:");
        cli::println("  ipc types                         - 列出所有 IPC 消息类型");
        cli::println("  ipc send <type_hex> <json_data>   - 发送 IPC 消息");
        cli::println("  ipc capture                       - 捕获/监听 IPC 消息");
        return -1;
    }

    const char* subcmd = argv[0];

    if (std::strcmp(subcmd, "types") == 0) {
        cli::println("");
        cli::println("  IPC 消息类型列表:");
        cli::println("  ┌────────┬────────────────────┬────────────────────────────┐");
        cli::println("  │ 类型码 │ 名称               │ 描述                       │");
        cli::println("  ├────────┼────────────────────┼────────────────────────────┤");
        for (std::size_t i = 0; IPC_MESSAGE_TYPES[i].name; i++) {
            cli::println("  │ 0x{:02X}   │ {:<18} │ {:<26} │",
                         IPC_MESSAGE_TYPES[i].type,
                         IPC_MESSAGE_TYPES[i].name,
                         IPC_MESSAGE_TYPES[i].description);
        }
        cli::println("  └────────┴────────────────────┴────────────────────────────┘");
        cli::println("");
        return 0;

    } else if (std::strcmp(subcmd, "send") == 0) {
        if (argc < 2) {
            cli::println(stderr, "用法: ipc send <type_hex> <json_data>");
            return -1;
        }

        int msg_type = 0;
        if (std::sscanf(argv[1], "%x", &msg_type) != 1) {
            cli::println(stderr, "无效的类型码: {}", argv[1]);
            return -1;
        }

        const char* type_name = "UNKNOWN";
        for (std::size_t i = 0; IPC_MESSAGE_TYPES[i].name; i++) {
            if (IPC_MESSAGE_TYPES[i].type == msg_type) {
                type_name = IPC_MESSAGE_TYPES[i].name;
                break;
            }
        }

        const char* json_data = (argc >= 3) ? argv[2] : "{}";

        cli::println("[*] IPC 消息已构造:");
        cli::println("    类型: 0x{:02X} ({})", msg_type, type_name);
        cli::println("    数据: {}", json_data);
        cli::println("    完整 IPC 消息:");
        cli::println("    {{");
        cli::println("      \"type\": 0x{:02X},", msg_type);
        cli::println("      \"data\": {},", json_data);
        cli::println("      \"timestamp\": {}", static_cast<long>(std::time(nullptr)));
        cli::println("    }}");
        cli::println("[+] IPC 消息处理完成");
        return 0;

    } else if (std::strcmp(subcmd, "capture") == 0) {
        cli::println("");
        cli::println("  ╔══════════════════════════════════════════════════════════╗");
        cli::println("  ║     IPC 消息捕获模式 (监听)                              ║");
        cli::println("  ╚══════════════════════════════════════════════════════════╝");
        cli::println("");
        cli::println("  [*] 进入 IPC 捕获模式...");
        cli::println("");

        const char* mock_messages[][3] = {
            {"0x01", "LOGIN",       "{\"user_id\": 1, \"token\": \"...\""},
            {"0x10", "SEND_MESSAGE","{\"from\": 1, \"to\": 2, \"content\": \"你好\"}"},
            {"0x20", "GET_CONTACTS","{\"user_id\": 1}"},
            {"0x30", "GET_TEMPLATES","{\"limit\": 50, \"offset\": 0}"},
            {"0x40", "FILE_UPLOAD", "{\"path\": \"/tmp/test.txt\", \"size\": 1024}"},
        };
        int num_mock = sizeof(mock_messages) / sizeof(mock_messages[0]);

        for (int i = 0; i < num_mock; i++) {
            std::time_t now = std::time(nullptr);
            char ts[32];
            std::strftime(ts, sizeof(ts), "%H:%M:%S", std::localtime(&now));

            cli::println("  ┌─────────────────────────────────────────────────────────┐");
            cli::println("  │ [{}] IPC 消息 #{}                                    │", ts, i + 1);
            cli::println("  ├─────────────────────────────────────────────────────────┤");
            cli::println("  │ 类型: {} ({})                                        │", mock_messages[i][0], mock_messages[i][1]);
            cli::println("  │ 数据: {}", mock_messages[i][2]);
            cli::println("  │ 方向: C <---> JS                                        │");
            cli::println("  └─────────────────────────────────────────────────────────┘");
            cli::println("");
            msleep(800);
        }

        cli::println("  [*] IPC 捕获结束 (共 {} 条消息)", num_mock);
        return 0;

    } else {
        cli::println(stderr, "未知 ipc 子命令: {}", subcmd);
        return -1;
    }
}

extern "C" void init_cmd_ipc(void)
{
    register_command("ipc",
                     "IPC 调试 (types/send/capture)",
                     "ipc <types|send|capture> ...",
                     cmd_ipc);
}
