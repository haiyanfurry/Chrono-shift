.PHONY: all server client rust_server rust_client clean

all: rust_server rust_client server client

# Rust 安全模块编译
rust_server:
	cd server/security && cargo build --release

rust_client:
	cd client/security && cargo build --release

# C 后端编译
server: rust_server
	cd server && cmake -B build && cmake --build build

client: rust_client
	cd client && cmake -B build && cmake --build build

# NSIS 安装包
installer: server client
	makensis installer/server_installer.nsi
	makensis installer/client_installer.nsi

clean:
	rm -rf server/build client/build
	cd server/security && cargo clean
	cd client/security && cargo clean
