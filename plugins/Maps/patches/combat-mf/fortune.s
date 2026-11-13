.intel_syntax noprefix
.text
.global store_mf,reset_mf,stat_mf,fortune_marker,mf_table
store_mf:
    push r8
    cmp byte ptr [rcx],0
    je store_original
    mov r8d,[rcx+0x18]
    cmp r8d,500
    ja store_original
    lea r10,[rip+mf_table]
    movzx edx,word ptr [rcx+4]
    cmp edx,512
    jae store_boss
    mov [r10+rdx*4],r8d
store_boss:
    movzx edx,word ptr [rcx+6]
    cmp edx,512
    jae store_original
    mov [r10+rdx*4],r8d
store_original:
    pop r8
    mov [rsp+8],rcx
    jmp store_continue
reset_mf:
    lea rax,[rip+mf_table]
    mov ecx,512
reset_loop:
    mov dword ptr [rax],0
    add rax,4
    dec ecx
    jnz reset_loop
    lea rax,[rip+xp_table]
    jmp reset_continue
fortune_marker:
    mov edx,1
    mov [rax+rcx+0xc],edx
    jmp fortune_continue
stat_mf:
    cmp dword ptr [rsp+0xa8],80
    je read_mf
    cmp dword ptr [rsp+0xa8],85
    jmp xp_stat_continue
read_mf:
    cmp word ptr [rsp+0xb0],0
    jne stat_original
    mov rcx,[rsp+0xa0]
    test rcx,rcx
    jz stat_original
    cmp dword ptr [rcx],0
    jne stat_original
    mov rax,[rip+get_level]
    test rax,rax
    jz stat_original
    sub rsp,0x20
    call rax
    add rsp,0x20
    cmp eax,175
    jb stat_original
    cmp eax,294
    ja stat_original
    lea r10,[rip+mf_table]
    mov edx,[r10+rax*4]
    test edx,edx
    jz stat_original
    mov [rsp+0x20],edx
    mov rcx,[rsp+0xa0]
    sub rsp,0x20
    call fortune_contribution
    add rsp,0x20
    test eax,eax
    jz stat_original
    mov edx,[rsp+0x20]
    mov rcx,[rsp+0x30]
    sub rcx,rax
    lea rax,[rcx+rdx]
    jmp stat_epilogue
fortune_contribution:
    xor eax,eax
    mov r8,[rcx+0x88]
    test r8,r8
    jz contribution_done
    test dword ptr [r8+0x1c],0x80000000
    jz contribution_done
    mov r8,[r8+0x90]
    mov r9d,256
list_loop:
    test r8,r8
    jz contribution_done
    cmp dword ptr [r8+0x20],299
    jne next_list
    test dword ptr [r8+0x1c],0x2000
    jnz next_list
    mov r10d,0x30
    test dword ptr [r8+0x1c],0x80000000
    jz list_span
    mov r10d,0xa8
list_span:
    mov rdx,[r8+r10+8]
    cmp rdx,4096
    ja contribution_invalid
    test rdx,rdx
    jz next_list
    mov r11,[r8+r10]
    test r11,r11
    jz contribution_invalid
    mov r10,0x5000000000
entry_loop:
    cmp [r11],r10
    jne next_entry
    mov rcx,[r11+8]
    cmp rcx,1
    jl contribution_invalid
    cmp rcx,1023
    ja contribution_invalid
    add rax,rcx
    jmp next_list
next_entry:
    add r11,16
    dec rdx
    jnz entry_loop
next_list:
    mov r8,[r8+0x68]
    dec r9d
    jnz list_loop
    test r8,r8
    jnz contribution_invalid
contribution_done:
    ret
contribution_invalid:
    xor eax,eax
    ret
.balign 16
mf_table:
    .zero 2048
