.intel_syntax noprefix
.text
    mov r11, [rip + 0x112189f]
    test r11, r11
    jz native
    mov r10, [rsp]
    sub r10, [r11 + 0x18]
    cmp r10, 0x1a569c
    je check_mage
    cmp r10, 0x1a5789
    jne native
check_mage:
    test rcx, rcx
    jz native
    test rdx, rdx
    jz native
    cmp dword ptr [rcx], 1
    jne native
    cmp dword ptr [rcx + 4], 364
    jne native
    cmp dword ptr [rcx + 12], 4
    jne native
    cmp dword ptr [rdx + 16], 4
    jne native
    mov r11, [rdx]
    test r11, r11
    jz native
    cmp word ptr [r11], 338
    jne native
    mov eax, 5
    ret
native:
