// client/security/build.rs
// NASM 编译脚本 — 将 obfuscate.asm 编译为 .obj 并链接到 Rust 库
//
// 前提: NASM 3.01+ 在 PATH 中可用
// 你唯一需要做的是: 保证 obfuscate.asm 语法正确

fn main() {
    let nasm = "nasm";

    let status = std::process::Command::new(nasm)
        .args(&[
            "-f", "win64",
            "-o",
            "asm\\obfuscate.obj",
            "asm\\obfuscate.asm",
        ])
        .status()
        .expect("NASM 未找到，请确保 nasm 在 PATH 中");
    assert!(status.success(), "NASM 编译 obfuscate.asm 失败");

    println!("cargo:rustc-link-search=native={}",
             std::env::current_dir().unwrap().display());
    println!("cargo:rustc-link-lib=static=obfuscate");

    // 当 asm 文件变更时重新编译
    println!("cargo:rerun-if-changed=asm/obfuscate.asm");
}
