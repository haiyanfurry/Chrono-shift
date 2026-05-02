; client/security/asm/obfuscate.asm
; NASM 3.01 — win64 COFF
; ChronoStream v1 — 自研对称流密码
; DEBUG VERSION — 逐步添加: ksa_init

section .data
    sbox:   times 256 db 0
    state:  times 8 db 0

section .text

; ============================================================
; KSA — 密钥调度初始化
; ============================================================
ksa_init:
    ; ── 阶段 1: 初始化 S-Box 为单位映射 ──
    xor     eax, eax
    lea     r11, [rel sbox]
.init_loop:
    mov     byte [r11 + rax], al
    inc     al
    jnz     .init_loop

    ; ── 阶段 2: 3 轮 Fisher-Yates 洗牌 ──
    xor     r12d, r12d          ; pass 计数器
    lea     r10, [rel sbox]     ; sbox 基址

.pass_loop:
    xor     ebx, ebx            ; j = 0
    xor     ecx, ecx            ; i = 0

.inner_loop:
    movzx   eax, byte [r10 + rcx]   ; al = sbox[i]
    add     eax, ebx
    and     eax, 0xFF

    mov     edx, ecx
    cmp     r12d, 0
    je      .key_idx_mod             ; 第 1 轮
    cmp     r12d, 1
    je      .pass2_key

    ; 第 3 轮: (i * 7) % 64
    imul    edx, 7
    jmp     .key_idx_mod

.pass2_key:
    imul    edx, 3

.key_idx_mod:
    and     edx, 63

.key_idx_done:
    movzx   eax, byte [r8 + rdx]     ; al = key[idx]
    add     ebx, eax
    and     ebx, 0xFF

    ; swap(sbox[i], sbox[j])
    movzx   eax, byte [r10 + rcx]
    movzx   edx, byte [r10 + rbx]
    mov     byte [r10 + rcx], dl
    mov     byte [r10 + rbx], al

    inc     ecx
    jnz     .inner_loop

    inc     r12d
    cmp     r12d, 3
    jl      .pass_loop

    ; ── 阶段 3: 初始化 8 字节内部状态 ──
    lea     r11, [rel state]
    mov     rax, [r8 + 8]
    mov     [r11], rax

    ret

; ============================================================
; asm_obfuscate — 调试版: 调用 ksa_init 然后复制数据
; ============================================================
global asm_obfuscate
asm_obfuscate:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15

    test    rdx, rdx
    jz      .param_error
    test    rcx, rcx
    jz      .param_error
    test    r8, r8
    jz      .param_error
    test    r9, r9
    jz      .param_error

    ; ── 保存参数 ──
    mov     rsi, rcx                 ; rsi = data ptr
    mov     r13, rdx                 ; r13 = len
    mov     r14, r8                  ; r14 = key ptr
    mov     r15, r9                  ; r15 = out ptr

    ; ── 调用 KSA ──
    call    ksa_init

    ; ── 复制数据 (无加密) ──
    xor     ecx, ecx
.copy_loop:
    cmp     rcx, r13
    jae     .done
    movzx   eax, byte [rsi + rcx]
    mov     byte [r15 + rcx], al
    inc     rcx
    jmp     .copy_loop

.done:
    xor     eax, eax
    jmp     .cleanup

.param_error:
    mov     eax, -1

.cleanup:
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

global asm_deobfuscate
asm_deobfuscate:
    jmp     asm_obfuscate
