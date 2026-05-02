; client/security/asm/obfuscate.asm
; ChronoStream v1 — 完整加密算法
; 对称流密码 + S-Box + 多轮混淆

section .data
    sbox:   times 256 db 0
    state:  times 8 db 0

section .text

; ============================================================
; ksa_init — 密钥调度初始化
; 参数: r14 = key pointer (64 bytes)
; 使用: r10 = sbox base, r11 = state base
; 所有被修改的寄存器都是 volatile (caller-saved)
; ============================================================
ksa_init:
    push    rbx                     ; 保存 rbx (callee-saved, 用于swap临时值)

    ; --- Step 1: identity init sbox[0..255] = 0..255 ---
    lea     r10, [rel sbox]
    xor     eax, eax
.init_loop:
    mov     byte [r10 + rax], al
    inc     al
    jnz     .init_loop

    ; --- Step 2: init state[0..7] = 0..7 ---
    lea     r11, [rel state]
    xor     eax, eax
.init_state:
    mov     byte [r11 + rax], al
    inc     al
    cmp     al, 8
    jb      .init_state

    ; --- Step 3: 3-pass Fisher-Yates shuffle ---
    ; pass loop (8-bit counter)
    xor     cl, cl                  ; cl = pass counter (0,1,2)
.pass_loop:
    ; inner loop: i = 0..255
    xor     edx, edx                ; dl = i (8-bit counter)

.inner_loop:
    movzx   eax, byte [r10 + rdx]   ; al = sbox[i] (temp for swap)
    ; j = i
    mov     ebx, edx                ; bl = i

    ; j += key[i % 64]
    mov     edi, edx
    and     edi, 63
    movzx   edi, byte [r14 + rdi]   ; key[i % 64]
    add     ebx, edi

    ; j += key[(i*3) % 64]
    mov     edi, edx
    imul    edi, 3
    and     edi, 63
    movzx   edi, byte [r14 + rdi]   ; key[(i*3) % 64]
    add     ebx, edi

    ; j += key[(i*7) % 64]
    mov     edi, edx
    imul    edi, 7
    and     edi, 63
    movzx   edi, byte [r14 + rdi]   ; key[(i*7) % 64]
    add     ebx, edi

    ; j &= 0xFF
    and     ebx, 255

    ; swap sbox[i] <-> sbox[j]
    movzx   eax, byte [r10 + rdx]   ; al = sbox[i]
    movzx   edi, byte [r10 + rbx]   ; dil = sbox[j]
    mov     byte [r10 + rdx], dil   ; sbox[i] = sbox[j]
    mov     byte [r10 + rbx], al    ; sbox[j] = sbox[i]

    inc     dl                      ; i++ (8-bit, wraps at 256)
    jnz     .inner_loop

    inc     cl                      ; pass++
    cmp     cl, 3
    jb      .pass_loop

    pop     rbx
    ret

; ============================================================
; gen_keystream — 生成一个密钥流字节并更新 S-Box 状态
; 参数: r10 = sbox base, r11 = state base
; 返回: al = 密钥流字节
; 修改: eax, ecx, edx, edi (均为 volatile)
; ============================================================
gen_keystream:
    ; state[0]++
    movzx   ecx, byte [r11]         ; cl = state[0]
    inc     cl
    mov     byte [r11], cl

    ; Cascade: state[1] += state[0], state[2] += state[1], ...
    movzx   edx, byte [r11 + 1]    ; dl = state[1]
    add     edx, ecx                ; state[1] += state[0]
    mov     byte [r11 + 1], dl

    movzx   ecx, byte [r11 + 2]    ; cl = state[2]
    add     ecx, edx                ; state[2] += state[1]
    mov     byte [r11 + 2], cl

    movzx   edx, byte [r11 + 3]    ; dl = state[3]
    add     edx, ecx                ; state[3] += state[2]
    mov     byte [r11 + 3], dl

    movzx   ecx, byte [r11 + 4]    ; cl = state[4]
    add     ecx, edx                ; state[4] += state[3]
    mov     byte [r11 + 4], cl

    movzx   edx, byte [r11 + 5]    ; dl = state[5]
    add     edx, ecx                ; state[5] += state[4]
    mov     byte [r11 + 5], dl

    movzx   ecx, byte [r11 + 6]    ; cl = state[6]
    add     ecx, edx                ; state[6] += state[5]
    mov     byte [r11 + 6], cl

    movzx   edx, byte [r11 + 7]    ; dl = state[7]
    add     edx, ecx                ; state[7] += state[6]
    mov     byte [r11 + 7], dl

    ; s = sum(state[0..7]) & 0xFF
    xor     eax, eax
    add     al, byte [r11]
    add     al, byte [r11 + 1]
    add     al, byte [r11 + 2]
    add     al, byte [r11 + 3]
    add     al, byte [r11 + 4]
    add     al, byte [r11 + 5]
    add     al, byte [r11 + 6]
    add     al, byte [r11 + 7]
    ; al = s (already 8-bit, no mask needed)

    ; swap(sbox[state[0]], sbox[s])
    movzx   ecx, byte [r11]         ; cl = state[0]
    movzx   edx, al                 ; dl = s

    movzx   edi, byte [r10 + rcx]   ; edi = sbox[state[0]]
    movzx   eax, byte [r10 + rdx]   ; al = sbox[s]
    mov     byte [r10 + rcx], al    ; sbox[state[0]] = sbox[s]
    mov     byte [r10 + rdx], dil   ; sbox[s] = sbox[state[0]]

    ; return sbox[(sbox[state[0]] + sbox[s]) & 0xFF]
    ; edi = original sbox[state[0]], eax = original sbox[s]
    add     eax, edi                ; al = sbox[state[0]] + sbox[s]
    movzx   eax, byte [r10 + rax]   ; al = sbox[(sum) & 0xFF]
    ret

; ============================================================
; asm_obfuscate — 加密/混淆数据
; 参数: RCX = data ptr, RDX = len, R8 = key(64), R9 = out ptr
; 返回: RAX = 0 (成功), -1 (参数错误)
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

    ; 参数检查
    test    rdx, rdx
    jz      .param_error
    test    rcx, rcx
    jz      .param_error
    test    r8, r8
    jz      .param_error
    test    r9, r9
    jz      .param_error

    ; 保存参数到 callee-saved 寄存器
    mov     rsi, rcx                ; rsi = data ptr
    mov     r13, rdx                ; r13 = len
    mov     r14, r8                 ; r14 = key ptr
    mov     r15, r9                 ; r15 = out ptr

    ; 执行 KSA
    call    ksa_init

    ; 加载 sbox/state 基址到 r10/r11
    lea     r10, [rel sbox]
    lea     r11, [rel state]

    ; 逐字节加密
    xor     r12d, r12d              ; r12 = byte index
.encrypt_loop:
    cmp     r12, r13
    jae     .done

    ; 生成密钥流字节
    call    gen_keystream           ; al = keystream byte

    ; XOR 加密
    movzx   ecx, byte [rsi + r12]  ; cl = data[i]
    xor     ecx, eax               ; cl ^= keystream
    mov     byte [r15 + r12], cl   ; out[i] = encrypted

    inc     r12
    jmp     .encrypt_loop

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

; ============================================================
; asm_deobfuscate — 解密（与加密相同，XOR 对称）
; ============================================================
global asm_deobfuscate
asm_deobfuscate:
    jmp     asm_obfuscate
