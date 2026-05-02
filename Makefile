# ============================================================
# Chrono-shift 根项目 Makefile
# 子项目可独立构建:
#   cd client && cmake -B build && cmake --build build
# ============================================================

.PHONY: all client rust_client clean installer

all: rust_client client

# ---- Rust 安全模块 ----
rust_client:
	cd client/security && cargo build --release

# ---- C 客户端 (CMake, 仅 Windows) ----
client: rust_client
	cd client && cmake -B build && cmake --build build

# ---- CLI 工具 ----
cli_tools:
	cd client/tools && $(MAKE)

# ---- NSIS 安装包 (仅 Windows) ----
installer: client
	makensis installer/client_installer.nsi

# ---- 清理 ----
clean:
	rm -rf client/build client/out
	cd client/security && cargo clean 2>/dev/null || true
