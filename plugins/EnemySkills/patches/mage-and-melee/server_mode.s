.intel_syntax noprefix
.text
    mov r8, [rbp + 0x1b8]
    mov r14, r13
    mov r9d, r14d
    cmp r8b, 4
    jne native
    cmp r9d, 338
    jne native
    cmp dword ptr [rsi + 0x5c], 364
    jne native
    cmp dword ptr [rsi + 0x94], 338
    jne native
    cmp dword ptr [rsi + 0x90], 4
    jne native
    mov r8b, 5
native:
