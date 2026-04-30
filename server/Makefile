# ============================================================
# Chrono-shift Server — 独立 Makefile
# 支持: Linux + Windows (MinGW)
# TLS:   make HAS_TLS=1  启用 OpenSSL (需要安装开发库)
#        make            默认使用 TLS 桩 (无需 OpenSSL)
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

# 从通配符中排除 tls_*.c，按条件编译其中一个
SRCS     := $(filter-out $(SRCDIR)/tls_stub.c $(SRCDIR)/tls_server.c, $(wildcard $(SRCDIR)/*.c))

# ---- TLS 支持 ----
# 设置 HAS_TLS=1 启用 OpenSSL 加密 (需安装 libssl-dev)
# 默认使用无加密的 TLS 桩实现
ifdef HAS_TLS
    CFLAGS += -DHAS_TLS
    SRCS   += $(SRCDIR)/tls_server.c
    $(info [TLS]  启用 OpenSSL 加密支持)
else
    SRCS   += $(SRCDIR)/tls_stub.c
    $(info [TLS]  使用 TLS 桩实现 (无加密, make HAS_TLS=1 启用))
endif

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

# TLS 链接库 (仅在启用时添加)
ifdef HAS_TLS
    # MinGW 需要手动指定 MSYS2 OpenSSL 路径
    ifdef IS_WINDOWS
        CFLAGS  += -ID:/mys32/mingw64/include
        LDFLAGS += -LD:/mys32/mingw64/lib -lssl -lcrypto
    else
        LDFLAGS += -lssl -lcrypto
    endif
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

# 编译 C 代码
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

# ---- CLI 调试工具 ----
TOOLDIR       := tools
DEBUG_CLI_SRC := $(TOOLDIR)/debug_cli.c
DEBUG_CLI     := $(BUILDDIR)/debug_cli

# 编译调试 CLI
debug-cli: $(BUILDDIR)
	$(CC) $(CFLAGS) $(DEBUG_CLI_SRC) -o $(DEBUG_CLI) $(LDFLAGS)
	@echo "=== Debug CLI build complete: $(DEBUG_CLI) ==="

# ---- 压力测试工具 ----
STRESS_TEST_SRC := $(TOOLDIR)/stress_test.c
STRESS_TEST     := $(BUILDDIR)/stress_test

# 编译压力测试
stress-test: $(BUILDDIR)
	$(CC) $(CFLAGS) $(STRESS_TEST_SRC) -o $(STRESS_TEST) $(LDFLAGS)
	@echo "=== Stress test build complete: $(STRESS_TEST) ==="

# 仅清理构建产物
clean-build: clean
