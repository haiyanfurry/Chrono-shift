# ============================================================
# Chrono-shift Server — 独立 Makefile
# 支持: Linux + Windows (MinGW)
# TLS:  强制启用 OpenSSL (需要安装 libssl-dev / MSYS2 mingw-w64)
# ============================================================

.PHONY: all rust build clean run debug-cli

# 默认目标
all: rust build

# ---- 检测操作系统 ----
OS_NAME := $(shell uname -s 2>/dev/null || echo Windows)
ifneq (,$(findstring Linux,$(OS_NAME)))
    IS_LINUX := 1
else
    IS_WINDOWS := 1
endif

# ---- 编译器与标志 ----
CC       := gcc
CFLAGS   := -std=c99 -Wall -Wextra -pedantic -Iinclude
LDFLAGS  :=

# ---- 源文件 ----
SRCDIR   := src
INCDIR   := include
BUILDDIR := out

# tls_stub.c 已删除 (TLS 为强制，不再需要桩实现)
SRCS     := $(filter-out $(SRCDIR)/tls_stub.c, $(wildcard $(SRCDIR)/*.c))

# ---- TLS 支持 (强制启用) ----
# OpenSSL 是必需依赖，HTTPS-only 模式
# tls_server.c 已包含在通配符中 (因 tls_stub.c 被排除)
CFLAGS += -DHAS_TLS
$(info [TLS]  强制启用 OpenSSL 加密 (HTTPS only))

OBJS     := $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(SRCS))
TARGET   := $(BUILDDIR)/chrono-server

# ---- 平台特定设置 ----
ifdef IS_LINUX
    LDFLAGS += -lpthread -lm
    # Rust 安全库 (可选)
    RUST_LIB := security/target/release/libchrono_server_security.a
else
    # Windows (MinGW)
    LDFLAGS += -lws2_32
    # Rust 安全库 (可选)
    RUST_LIB := security/target/release/chrono_server_security.lib
endif

# TLS 链接库 (强制链接 OpenSSL)
# MinGW 需要手动指定 MSYS2 OpenSSL 路径
ifdef IS_WINDOWS
    CFLAGS  += -ID:/mys32/mingw64/include
    LDFLAGS += -LD:/mys32/mingw64/lib -lssl -lcrypto
else
    LDFLAGS += -lssl -lcrypto
endif

# 如果 Rust 安全库存在，链接它
ifneq (,$(wildcard $(RUST_LIB)))
    LDFLAGS += $(RUST_LIB)
    ifdef IS_WINDOWS
        LDFLAGS += -luserenv -ladvapi32
    endif
    CFLAGS += -DHAS_RUST_SECURITY
endif

# ---- 辅助: 创建构建目录 (跨平台) ----
$(BUILDDIR):
ifdef IS_WINDOWS
	if not exist "$(BUILDDIR)" mkdir "$(BUILDDIR)"
else
	mkdir -p $(BUILDDIR)
endif

# ---- 构建规则 ----

# 编译 Rust 安全模块
rust:
	cd security && cargo build --release

# 编译 C 代码 (TLS 强制启用, OpenSSL 必需)
build: $(BUILDDIR) $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)
	@echo "=== Build complete: $(TARGET) ==="

$(BUILDDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# 直接运行 (需要先 build)
run: build
	./$(TARGET)

# 清理
clean:
ifdef IS_WINDOWS
	if exist "$(BUILDDIR)" rmdir /s /q "$(BUILDDIR)"
	cd security && cargo clean 2>nul || cd .
else
	rm -rf $(BUILDDIR)
	cd security && cargo clean 2>/dev/null || true
endif

# ---- CLI 调试工具 (需要 OpenSSL + JSON 解析器) ----
TOOLDIR       := tools
DEBUG_CLI_SRC := $(TOOLDIR)/debug_cli.c
DEBUG_CLI     := $(BUILDDIR)/debug_cli

# 编译调试 CLI (需要 TLS + JSON 解析器)
DEBUG_CLI_DEPS := $(SRCDIR)/json_parser.c $(SRCDIR)/tls_server.c
debug-cli: $(BUILDDIR)
	$(CC) $(CFLAGS) $(DEBUG_CLI_SRC) $(DEBUG_CLI_DEPS) -o $(DEBUG_CLI) $(LDFLAGS)
	@echo "=== Debug CLI build complete: $(DEBUG_CLI) ==="

# ---- 压力测试工具 (需要 TLS + OpenSSL) ----
STRESS_TEST_SRC := $(TOOLDIR)/stress_test.c
STRESS_TEST     := $(BUILDDIR)/stress_test

# 编译压力测试 (需要 tls_server.c 提供客户端 TLS API)
# tls_server.c 同时提供 tls_client_init/connect 等客户端函数
STRESS_TEST_DEPS := $(SRCDIR)/tls_server.c
stress-test: $(BUILDDIR)
	$(CC) $(CFLAGS) $(STRESS_TEST_SRC) $(STRESS_TEST_DEPS) -o $(STRESS_TEST) $(LDFLAGS)
	@echo "=== Stress test build complete: $(STRESS_TEST) ==="

# 仅清理构建产物
clean-build: clean
