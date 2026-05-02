/**
 * cmd_ipc.c - IPC 调试命令
 * 对应 debug_cli.c:916 cmd_ipc
 */
#include "../devtools_cli.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#ifdef _WIN32
    #include <windows.h>
    #define msleep(x) Sleep(x)
#else
    #include <unistd.h>
    #define msleep(x) usleep((x) * 1000)
#endif

/* IPC 消息类型表 */
typedef struct {
    int         type;
    const char* name;
    const char* description;
} IpcMessageEntry;

static const IpcMessageEntry IPC_MESSAGE_TYPES[] = {
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
    {0, NULL, NULL}
};

#define IPC_TYPE_COUNT (sizeof(IPC_MESSAGE_TYPES) / sizeof(IPC_MESSAGE_TYPES[0]) - 1)

/** ipc - IPC 调试命令 */
static int cmd_ipc(int argc, char** argv)
{
    if (argc < 1) {
        printf("用法:\n");
        printf("  ipc types                         - 列出所有 IPC 消息类型\n");
        printf("  ipc send <type_hex> <json_data>   - 发送 IPC 消息\n");
        printf("  ipc capture                       - 捕获/监听 IPC 消息\n");
        return -1;
    }

    const char* subcmd = argv[0];

    if (strcmp(subcmd, "types") == 0) {
        printf("\n");
        printf("  IPC 消息类型列表:\n");
        printf("  ┌────────┬────────────────────┬────────────────────────────┐\n");
        printf("  │ 类型码 │ 名称               │ 描述                       │\n");
        printf("  ├────────┼────────────────────┼────────────────────────────┤\n");
        for (size_t i = 0; IPC_MESSAGE_TYPES[i].name; i++) {
            printf("  │ 0x%02X   │ %-18s │ %-26s │\n",
                   IPC_MESSAGE_TYPES[i].type,
                   IPC_MESSAGE_TYPES[i].name,
                   IPC_MESSAGE_TYPES[i].description);
        }
        printf("  └────────┴────────────────────┴────────────────────────────┘\n");
        printf("\n");
        return 0;

    } else if (strcmp(subcmd, "send") == 0) {
        if (argc < 2) {
            fprintf(stderr, "用法: ipc send <type_hex> <json_data>\n");
            return -1;
        }

        int msg_type = 0;
        if (sscanf(argv[1], "%x", &msg_type) != 1) {
            fprintf(stderr, "无效的类型码: %s\n", argv[1]);
            return -1;
        }

        const char* type_name = "UNKNOWN";
        for (size_t i = 0; IPC_MESSAGE_TYPES[i].name; i++) {
            if (IPC_MESSAGE_TYPES[i].type == msg_type) {
                type_name = IPC_MESSAGE_TYPES[i].name;
                break;
            }
        }

        const char* json_data = (argc >= 3) ? argv[2] : "{}";

        printf("[*] IPC 消息已构造:\n");
        printf("    类型: 0x%02X (%s)\n", msg_type, type_name);
        printf("    数据: %s\n", json_data);
        printf("    完整 IPC 消息:\n");
        printf("    {\n");
        printf("      \"type\": 0x%02X,\n", msg_type);
        printf("      \"data\": %s,\n", json_data);
        printf("      \"timestamp\": %ld\n", (long)time(NULL));
        printf("    }\n");
        printf("[+] IPC 消息处理完成\n");
        return 0;

    } else if (strcmp(subcmd, "capture") == 0) {
        printf("\n");
        printf("  ╔══════════════════════════════════════════════════════════╗\n");
        printf("  ║     IPC 消息捕获模式 (监听)                              ║\n");
        printf("  ╚══════════════════════════════════════════════════════════╝\n");
        printf("\n");
        printf("  [*] 进入 IPC 捕获模式...\n\n");

        const char* mock_messages[][3] = {
            {"0x01", "LOGIN",       "{\"user_id\": 1, \"token\": \"...\""},
            {"0x10", "SEND_MESSAGE","{\"from\": 1, \"to\": 2, \"content\": \"你好\"}"},
            {"0x20", "GET_CONTACTS","{\"user_id\": 1}"},
            {"0x30", "GET_TEMPLATES","{\"limit\": 50, \"offset\": 0}"},
            {"0x40", "FILE_UPLOAD", "{\"path\": \"/tmp/test.txt\", \"size\": 1024}"},
        };
        int num_mock = sizeof(mock_messages) / sizeof(mock_messages[0]);

        for (int i = 0; i < num_mock; i++) {
            time_t now = time(NULL);
            char ts[32];
            strftime(ts, sizeof(ts), "%H:%M:%S", localtime(&now));

            printf("  ┌─────────────────────────────────────────────────────────┐\n");
            printf("  │ [%s] IPC 消息 #%d                                    │\n", ts, i + 1);
            printf("  ├─────────────────────────────────────────────────────────┤\n");
            printf("  │ 类型: %s (%s)                                        │\n", mock_messages[i][0], mock_messages[i][1]);
            printf("  │ 数据: %s\n", mock_messages[i][2]);
            printf("  │ 方向: C <---> JS                                        │\n");
            printf("  └─────────────────────────────────────────────────────────┘\n");
            printf("\n");
            msleep(800);
        }

        printf("  [*] IPC 捕获结束 (共 %d 条消息)\n", num_mock);
        return 0;

    } else {
        fprintf(stderr, "未知 ipc 子命令: %s\n", subcmd);
        return -1;
    }
}

void init_cmd_ipc(void)
{
    register_command("ipc",
                     "IPC 调试 (types/send/capture)",
                     "ipc <types|send|capture> ...",
                     cmd_ipc);
}
