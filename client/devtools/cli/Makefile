# ============================================================
# Chrono-shift 开发者模式 CLI — 独立 Makefile
# ============================================================
# 使用方法:
#   make           编译 chrono-devtools
#   make clean     清理构建产物
#   make NO_TLS=1  编译不带 TLS 支持的版本 (仅 HTTP)
# ============================================================

CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -pedantic
LDFLAGS =

# 头文件搜索路径
CFLAGS += -I.

UNAME_S := $(shell uname -s 2>/dev/null || echo Windows)

# --- 平台检测 ---
ifeq ($(UNAME_S), Windows)
    # Windows (MinGW 或 MSVC)
    CFLAGS += -D_WIN32_WINNT=0x0601
    LDFLAGS += -lws2_32
    OUT_DIR = .
    EXT = .exe
    RM = del /Q
else
    # Linux / macOS
    LDFLAGS += -lm
    OUT_DIR = .
    EXT =
    RM = rm -f
endif

# --- TLS 支持 ---
ifneq ($(NO_TLS), 1)
    # 启用 TLS: 需要 OpenSSL 和 tls_client.c
    TLS_SRC = ../../src/network/tls_client.c
    TLS_INC = -I../../include

    ifneq ($(OPENSSL_DIR),)
        CFLAGS += -I$(OPENSSL_DIR)/include
        LDFLAGS += -L$(OPENSSL_DIR)/lib
    endif
    LDFLAGS += -lssl -lcrypto
else
    TLS_SRC =
    TLS_INC =
    CFLAGS += -DNO_TLS
endif

# --- 源文件 ---
CLI_SRCS = main.c net_http.c commands/init_commands.c

# 所有 cmd_*.c 命令文件
CMD_SRCS := $(wildcard commands/cmd_*.c)

# 最终目标
TARGET = $(OUT_DIR)/chrono-devtools$(EXT)
OBJS = main.o net_http.o commands/init_commands.o $(patsubst commands/%.c, commands/%.o, $(notdir $(CMD_SRCS)))

# 如果启用 TLS, 添加 tls_client.o
ifneq ($(NO_TLS), 1)
    TLS_OBJ = tls_client.o
endif

.PHONY: all clean

all: $(TARGET)

# --- 编译规则 ---

$(TARGET): $(OBJS) $(TLS_OBJ)
	$(CC) $^ -o $@ $(LDFLAGS)

main.o: main.c devtools_cli.h
	$(CC) $(CFLAGS) -c main.c -o $@

net_http.o: net_http.c devtools_cli.h
	$(CC) $(CFLAGS) -c net_http.c -o $@

commands/init_commands.o: commands/init_commands.c devtools_cli.h
	$(CC) $(CFLAGS) -c commands/init_commands.c -o $@

commands/%.o: commands/%.c devtools_cli.h
	$(CC) $(CFLAGS) -c $< -o $@

# TLS 客户端 (来自 client/src/network/)
tls_client.o: ../../src/network/tls_client.c ../../include/tls_client.h
	$(CC) $(CFLAGS) $(TLS_INC) -c ../../src/network/tls_client.c -o $@

# --- 清理 ---
clean:
	$(RM) $(OBJS) $(TLS_OBJ) $(TARGET)

# --- 显示帮助 ---
help:
	@echo "Chrono-shift 开发者模式 CLI 构建系统"
	@echo ""
	@echo "目标:"
	@echo "  make         编译 $(TARGET)"
	@echo "  make clean   清理构建产物"
	@echo "  make NO_TLS=1 编译不带 TLS 的版本"
	@echo "  make help    显示此帮助信息"
	@echo ""
	@echo "平台: $(UNAME_S)"
	@echo "TLS:  $([ $(NO_TLS) ] && echo '禁用' || echo '启用')"
