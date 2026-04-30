# ============================================================
# Chrono-shift 根项目 Makefile
# 子项目可独立构建:
#   cd server && make all
#   cd client && cmake -B build && cmake --build build
# ============================================================

.PHONY: all server client rust_server rust_client clean installer

all: rust_server rust_client server client

# ---- Rust 安全模块 ----
rust_server:
	cd server/security && cargo build --release

rust_client:
	cd client/security && cargo build --release

# ---- C 后端 (CMake) ----
server: rust_server
	cd server && cmake -B build && cmake --build build

# ---- C 客户端 (CMake, 仅 Windows) ----
client: rust_client
	cd client && cmake -B build && cmake --build build

# ---- NSIS 安装包 (仅 Windows) ----
installer: server client
	makensis installer/server_installer.nsi
	makensis installer/client_installer.nsi

# ---- 清理 ----
clean:
	rm -rf server/build client/build
	cd server/security && cargo clean 2>/dev/null || true
	cd client/security && cargo clean 2>/dev/null || true
