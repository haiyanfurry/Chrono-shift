; client/security/asm/obfuscate.asm
; NASM 3.01 — win64 COFF
; ChronoStream v1 — 自研对称流密码
;
; ╔══════════════════════════════════════════════════════════════╗
; ║  算法: 对称流密码 + 运行时动态 S-Box + 8 轮混淆             ║
; ║                                                            ║
; ║  结构:                                                      ║
; ║    1. KSA — 3 轮 Fisher-Yates, 64 字节密钥动态生成 S-Box   ║
; ║    2. 状态初始化 — 8 字节内部状态 (key[8:16])              ║
; ║    3. 密钥流生成 — 8 轮级联 S-Box 查找 + 状态反馈           ║
; ║    4. 输出 — output[i] = input[i] XOR keystream[i]          ║
; ║                                                            ║
; ║  密钥: 64 字节 (512 位), R8 传入                           ║
; ║  接口: RCX=data, RDX=len, R8=key(64B), R9=out → RAX=0/-1  ║
; ╚══════════════════════════════════════════════════════════════╝

section .data
    ; S-Box 缓冲区 — 运行时由密钥动态生成
    ; 256 字节, 双射置换表
    sbox:   times 256 db 0

    ; 8 字节内部状态缓冲区
    state:  times 8 db 0

section .text

; ============================================================
; KSA — 密钥调度初始化 (Key Scheduling Algorithm)
; 功能: 用 64 字节密钥动态生成 256 字节 S-Box + 8 字节状态
; 输入: R8 = 64 字节密钥指针
; 输出: sbox[] 已初始化, state[] 已填充
; 破坏: RAX, RBX, RCX, RDX, R10, R11, R12
; ============================================================
ksa_init:
    ; ── 阶段 1: 初始化 S-Box 为单位映射 [0, 1, 2, ..., 255] ──
    xor     eax, eax
    lea     r11, [rel sbox]
.init_loop:
    mov     byte [r11 + rax], al
    inc     al
    jnz     .init_loop

    ; ── 阶段 2: 3 轮 Fisher-Yates 洗牌 ──
    ;   第 1 轮: key[i % 64]
    ;   第 2 轮: key[(i * 3) % 64]
    ;   第 3 轮: key[(i * 7) % 64]
    xor     r12d, r12d          ; r12 = pass 计数器 (0/1/2)
    lea     r10, [rel sbox]     ; r10 = sbox 基址

.pass_loop:
    xor     ebx, ebx            ; j = 0
    xor     ecx, ecx            ; i = 0

.inner_loop:
    movzx   eax, byte [r10 + rcx]   ; al = sbox[i]
    add     eax, ebx                 ; al += j
    and     eax, 0xFF                ; & 0xFF

    ; 计算密钥索引: idx = (i * multiplier) % 64
    mov     edx, ecx                 ; edx = i
    cmp     r12d, 0
    je      .key_idx_done            ; 第 1 轮: i % 64 (即 ecx & 63)
    cmp     r12d, 1
    je      .pass2_key

    ; 第 3 轮: (i * 7) % 64
    imul    edx, 7
    jmp     .key_idx_mod

.pass2_key:
    ; 第 2 轮: (i * 3) % 64
    imul    edx, 3

.key_idx_mod:
    and     edx, 63

.key_idx_done:
    movzx   eax, byte [r8 + rdx]     ; al = key[idx]
    add     ebx, eax                 ; j += key[idx]
    and     ebx, 0xFF                ; j &= 0xFF

    ; swap(sbox[i], sbox[j])
    movzx   eax, byte [r10 + rcx]    ; al = sbox[i]
    movzx   edx, byte [r10 + rbx]    ; dl = sbox[j]
    mov     byte [r10 + rcx], dl     ; sbox[i] = sbox[j]
    mov     byte [r10 + rbx], al     ; sbox[j] = orig sbox[i]

    inc     ecx
    jnz     .inner_loop              ; i = 0..255

    inc     r12d
    cmp     r12d, 3
    jl      .pass_loop

    ; ── 阶段 3: 初始化 8 字节内部状态 (key[8:16]) ──
    lea     r11, [rel state]
    mov     rax, [r8 + 8]            ; 加载 key[8..15] 到 rax
    mov     [r11], rax                ; 写入 state[0..7]

    ret

; ============================================================
; gen_keystream — 生成 1 字节密钥流 (8 轮混淆)
; 功能: 更新 8 字节状态, 生成 1 字节密钥流
; 输入: R10 = sbox 基址, R11 = state 基址
; 输出: AL = 1 字节密钥流
; 破坏: RAX, RDX
; ============================================================
gen_keystream:
    ; ── 更新 8 字节状态 ──
    ; state[0] = (state[0] + 1) & 0xFF
    movzx   eax, byte [r11]
    inc     al
    mov     byte [r11], al

    ; state[1] = sbox[(state[1] + state[0]) & 0xFF]
    movzx   eax, byte [r11 + 1]
    movzx   edx, byte [r11]          ; state[0] (已更新)
    add     eax, edx
    and     eax, 0xFF
    movzx   eax, byte [r10 + rax]
    mov     byte [r11 + 1], al

    ; state[2] = (state[2] + state[1]) & 0xFF
    movzx   eax, byte [r11 + 2]
    movzx   edx, byte [r11 + 1]
    add     eax, edx
    and     eax, 0xFF
    mov     byte [r11 + 2], al

    ; state[3] = sbox[(state[3] + state[2]) & 0xFF]
    movzx   eax, byte [r11 + 3]
    movzx   edx, byte [r11 + 2]
    add     eax, edx
    and     eax, 0xFF
    movzx   eax, byte [r10 + rax]
    mov     byte [r11 + 3], al

    ; state[4] = (state[4] + state[3]) & 0xFF
    movzx   eax, byte [r11 + 4]
    movzx   edx, byte [r11 + 3]
    add     eax, edx
    and     eax, 0xFF
    mov     byte [r11 + 4], al

    ; state[5] = sbox[(state[5] + state[4]) & 0xFF]
    movzx   eax, byte [r11 + 5]
    movzx   edx, byte [r11 + 4]
    add     eax, edx
    and     eax, 0xFF
    movzx   eax, byte [r10 + rax]
    mov     byte [r11 + 5], al

    ; state[6] = (state[6] + state[5]) & 0xFF
    movzx   eax, byte [r11 + 6]
    movzx   edx, byte [r11 + 5]
    add     eax, edx
    and     eax, 0xFF
    mov     byte [r11 + 6], al

    ; state[7] = sbox[(state[7] + state[6]) & 0xFF]
    movzx   eax, byte [r11 + 7]
    movzx   edx, byte [r11 + 6]
    add     eax, edx
    and     eax, 0xFF
    movzx   eax, byte [r10 + rax]
    mov     byte [r11 + 7], al

    ; ── 8 轮混淆: 生成密钥流字节 ──
    ; ks = sbox[state[0]]
    movzx   eax, byte [r11]
    movzx   eax, byte [r10 + rax]

    ; 轮 2: ks = sbox[(ks + state[2]) & 0xFF]
    movzx   edx, byte [r11 + 2]
    add     eax, edx
    and     eax, 0xFF
    movzx   eax, byte [r10 + rax]

    ; 轮 3: ks = sbox[(ks + state[4]) & 0xFF]
    movzx   edx, byte [r11 + 4]
    add     eax, edx
    and     eax, 0xFF
    movzx   eax, byte [r10 + rax]

    ; 轮 4: ks = sbox[(ks + state[6]) & 0xFF]
    movzx   edx, byte [r11 + 6]
    add     eax, edx
    and     eax, 0xFF
    movzx   eax, byte [r10 + rax]

    ; 轮 5: ks = sbox[(ks + state[1]) & 0xFF]
    movzx   edx, byte [r11 + 1]
    add     eax, edx
    and     eax, 0xFF
    movzx   eax, byte [r10 + rax]

    ; 轮 6: ks = sbox[(ks + state[3]) & 0xFF]
    movzx   edx, byte [r11 + 3]
    add     eax, edx
    and     eax, 0xFF
    movzx   eax, byte [r10 + rax]

    ; 轮 7: ks = sbox[(ks + state[5]) & 0xFF]
    movzx   edx, byte [r11 + 5]
    add     eax, edx
    and     eax, 0xFF
    movzx   eax, byte [r10 + rax]

    ; 轮 8: ks = sbox[(ks + state[7]) & 0xFF]
    movzx   edx, byte [r11 + 7]
    add     eax, edx
    and     eax, 0xFF
    movzx   eax, byte [r10 + rax]

    ret

; ============================================================
; asm_obfuscate — 对称流密码加密
; RCX = data ptr, RDX = len, R8 = key(64B), R9 = out
; Returns: RAX = 0 成功, -1 失败
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

    ; ── 保存参数到被调用者保存的寄存器 ──
    ; 注意: 不能使用 r12 — ksa_init 会覆盖 r12 (pass 计数器)
    ;       rsi/rdi/r13/r14/r15 不受 ksa_init 影响
    mov     rsi, rcx                 ; rsi = data ptr
    mov     r13, rdx                 ; r13 = data length
    mov     r14, r8                  ; r14 = key ptr (64 bytes)
    mov     r15, r9                  ; r15 = out ptr

    ; ── KSA: 初始化 S-Box 和状态 ──
    ; 注意: ksa_init 使用 r8 作为 key ptr
    call    ksa_init

    ; ── 加密循环: 逐字节生成密钥流 XOR ──
    lea     r10, [rel sbox]          ; r10 = sbox 基址
    lea     r11, [rel state]         ; r11 = state 基址
    xor     ecx, ecx                 ; rcx = 位置计数器

.enc_loop:
    ; 检查是否处理完所有字节
    cmp     rcx, r13
    jae     .done

    ; 生成 1 字节密钥流
    call    gen_keystream            ; al = keystream byte

    ; XOR 密钥流到输入字节
    movzx   edx, byte [rsi + rcx]    ; dl = data[rcx]  (rsi 不受 ksa_init 影响)
    xor     edx, eax                 ; dl ^= keystream
    mov     byte [r15 + rcx], dl     ; out[rcx] = result

    inc     rcx                      ; 位置 +1
    jmp     .enc_loop

    ; ── 成功返回 ──
.done:
    xor     eax, eax                 ; RAX = 0 (成功)
    jmp     .cleanup

.param_error:
    mov     eax, -1                  ; RAX = -1 (失败)

.cleanup:
    ; ── 密钥清理: 清零 S-Box 和状态（防内存 dump） ──
    ; 仅当成功加密时才做清理（避免在出错时破坏其他数据）
    cmp     eax, 0
    jne     .skip_cleanup

    lea     rdi, [rel sbox]
    xor     eax, eax
    mov     ecx, 256
    rep     stosb                    ; 清零 sbox[256]

    lea     rdi, [rel state]
    mov     ecx, 8
    rep     stosb                    ; 清零 state[8]

.skip_cleanup:
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
; 对称流密码: 解密 == 加密
; ============================================================
global asm_deobfuscate
asm_deobfuscate:
    jmp     asm_obfuscate
