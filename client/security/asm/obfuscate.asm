; client/security/asm/obfuscate.asm
; NASM 3.01 — win64 COFF
; ChronoStream v1 — 自研对称流密码
; DEBUG VERSION — 逐步添加功能以隔离崩溃

section .text

; ============================================================
; asm_obfuscate — 对称流密码加密 (调试版)
; ============================================================
global asm_obfuscate
asm_obfuscate:
    ; ── 保存被调用者保存的寄存器 ──
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15

    ; ── 参数检查 ──
    test    rdx, rdx
    jz      .param_error             ; len == 0 → 失败

    test    rcx, rcx
    jz      .param_error             ; data == NULL → 失败

    test    r8, r8
    jz      .param_error             ; key == NULL → 失败

    test    r9, r9
    jz      .param_error             ; out == NULL → 失败

    ; ── 简单复制: 在确定 ASM 调用约定正确前先只做 memcpy ──
    mov     rsi, rcx                 ; rsi = src (data ptr)
    mov     rdi, r9                  ; rdi = dst (out ptr)
    mov     rcx, rdx                 ; rcx = count (data length)
    
    ; 逐字节复制
    xor     eax, eax
.copy_loop:
    cmp     rax, rcx
    jae     .done
    movzx   r10d, byte [rsi + rax]
    mov     byte [rdi + rax], r10b
    inc     rax
    jmp     .copy_loop

    ; ── 成功返回 ──
.done:
    xor     eax, eax                 ; RAX = 0 (成功)
    jmp     .cleanup

.param_error:
    mov     eax, -1                  ; RAX = -1 (失败)

.cleanup:
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; ============================================================
; asm_deobfuscate — 对称流密码解密
; ============================================================
global asm_deobfuscate
asm_deobfuscate:
    jmp     asm_obfuscate
