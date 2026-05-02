// client/security/build.rs
// NASM 编译脚本 — 将 obfuscate.asm 编译为 .obj 并链接到 Rust 库
//
// 前提: NASM 3.01+ 在 PATH 中可用
// 你唯一需要做的是: 保证 obfuscate.asm 语法正确

fn main() {
    let nasm = "nasm";

    // 获取包根目录
    let pkg_root = std::env::current_dir().expect("无法获取当前目录");
    let asm_dir = pkg_root.join("asm");

    let asm_src = asm_dir.join("obfuscate.asm");
    let obj_file = asm_dir.join("obfuscate.obj");

    // 编译 NASM
    let status = std::process::Command::new(nasm)
        .args(&[
            "-f", "win64",
            "-o",
            &obj_file.to_string_lossy(),
            &asm_src.to_string_lossy(),
        ])
        .status()
        .expect("NASM 未找到，请确保 nasm 在 PATH 中");
    assert!(status.success(), "NASM 编译 obfuscate.asm 失败");

    // 将 .obj 文件直接传递给链接器
    // MSVC link.exe 支持直接链接 .obj 目标文件（不需要 .lib 包装）
    // 使用绝对路径确保链接器能找到文件
    println!("cargo:rustc-link-arg={}", obj_file.to_string_lossy());

    // 当 asm 文件变更时重新编译
    println!("cargo:rerun-if-changed=asm/obfuscate.asm");
}
