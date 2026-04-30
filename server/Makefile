# ============================================================
# Chrono-shift Server — 独立 Makefile
# 支持: Linux + Windows (MinGW)
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
BUILDDIR := build

SRCS     := $(wildcard $(SRCDIR)/*.c)
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

# 如果 Rust 安全库存在，链接它
ifneq (,$(wildcard $(RUST_LIB)))
    LDFLAGS += $(RUST_LIB)
    ifdef IS_WINDOWS
        LDFLAGS += -luserenv -ladvapi32
    endif
    CFLAGS += -DHAS_RUST_SECURITY
endif

# ---- 构建规则 ----

# 编译 Rust 安全模块
rust:
	cd security && cargo build --release

# 编译 C 代码
build: $(BUILDDIR) $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)
	@echo "=== Build complete: $(TARGET) ==="

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(BUILDDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# 直接运行 (需要先 build)
run: build
	./$(TARGET)

# 清理
clean:
	rm -rf $(BUILDDIR)
	cd security && cargo clean 2>/dev/null || true

# ---- CLI 调试工具 ----
TOOLDIR       := tools
DEBUG_CLI_SRC := $(TOOLDIR)/debug_cli.c
DEBUG_CLI     := $(BUILDDIR)/debug_cli

# 编译调试 CLI
debug-cli: $(BUILDDIR)
	$(CC) $(CFLAGS) $(DEBUG_CLI_SRC) -o $(DEBUG_CLI) $(LDFLAGS)
	@echo "=== Debug CLI build complete: $(DEBUG_CLI) ==="

# 仅清理构建产物
clean-build:
	rm -rf $(BUILDDIR)
