# ============================================================
# Chrono-shift CLI 工具 Makefile
# ============================================================
# 使用:
#   make debug_cli      - 编译调试 CLI
#   make stress_test    - 编译压力测试工具
#   make all            - 编译所有工具
#   make clean          - 清理构建产物
# ============================================================

CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -pedantic -I../include
LDFLAGS = -lssl -lcrypto

UNAME_S := $(shell uname -s 2>/dev/null || echo Windows)

ifeq ($(UNAME_S), Windows)
    # Windows (MinGW)
    # 如果 OpenSSL 安装在非标准路径，请设置 OPENSSL_DIR
    # 例如: make OPENSSL_DIR=D:/mys32/mingw64
    ifneq ($(OPENSSL_DIR),)
        CFLAGS += -I$(OPENSSL_DIR)/include
        LDFLAGS += -L$(OPENSSL_DIR)/lib
    endif
    LDFLAGS += -lws2_32
    OUT_DIR = .
    EXT = .exe
else
    # Linux
    LDFLAGS += -lpthread -lm
    OUT_DIR = .
    EXT =
endif

.PHONY: all debug_cli stress_test clean

all: debug_cli stress_test

debug_cli: debug_cli.c
	$(CC) $(CFLAGS) debug_cli.c -o $(OUT_DIR)/chrono-cli$(EXT) $(LDFLAGS)

stress_test: stress_test.c
	$(CC) $(CFLAGS) stress_test.c -o $(OUT_DIR)/stress_test$(EXT) $(LDFLAGS)

clean:
	rm -f $(OUT_DIR)/chrono-cli$(EXT) $(OUT_DIR)/stress_test$(EXT)
