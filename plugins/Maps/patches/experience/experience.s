.intel_syntax noprefix
.text
.global item_roll, client_roll, record_generation, store_levels, reset_levels, stat_bonus, tooltip_bonus, hud_bonus
.global roll_xp, read_xp, xp_table
# EAX=seed, ECX=tier. Deterministic independent hash, inclusive range.
roll_xp:
    dec ecx
    cmp ecx,5
    ja xp_zero
    xor eax,0x6d2b79f5
    mov edx,eax
    shr edx,16
    xor eax,edx
    imul eax,eax,0x7feb352d
    mov edx,eax
    shr edx,15
    xor eax,edx
    imul eax,eax,0x846ca68b
    mov edx,eax
    shr edx,16
    xor eax,edx
    lea r11,[rip+xp_ranges]
    mov r8d,[r11+rcx*8]
    mov r9d,[r11+rcx*8+4]
    sub r9d,r8d
    inc r9d
    xor edx,edx
    div r9d
    lea eax,[rdx+r8]
    ret
xp_zero:
    xor eax,eax
    ret
# RCX=roll, EAX=XP, word12 is padding; preserve the packet rarity marker in bit7.
encode_xp:
    mov edx,eax
    and eax,127
    and edx,0xffffff80
    shl edx,1
    or eax,edx
    or eax,128
    mov [rcx+0x12],ax
    ret
# RCX=roll, return XP. Magic maps are explicitly zero.
read_xp:
    xor eax,eax
    test byte ptr [rcx+0x12],128
    jz read_done
    movzx edx,word ptr [rcx+0x12]
    mov eax,edx
    and eax,127
    shr edx,1
    and edx,0x7f80
    or eax,edx
read_done:
    ret
item_roll:
    mov rax,[rsp+0xe0]
    mov rdx,[rsp+0xe8]
    mov word ptr [rax+0x12],0
    cmp dword ptr [rdx+0x34],6
    jne item_done
    cmp byte ptr [rax],0
    je item_done
    # Keep existing rare density/MF scaling exactly once.
    shl dword ptr [rax+0x18],1
    mov edx,[rax+0x1c]
    cmp edx,1000
    jb item_density_done
    lea edx,[rdx+rdx-1000]
    mov ecx,65535
    cmp edx,ecx
    cmova edx,ecx
    mov [rax+0x1c],edx
item_density_done:
    movzx ecx,byte ptr [rax+2]
    mov eax,[rsp+0xf0]
    sub rsp,0x20
    call roll_xp
    add rsp,0x20
    mov rcx,[rsp+0xe0]
    sub rsp,0x20
    call encode_xp
    add rsp,0x20
item_done:
    mov rax,[rsp+0xe0]
    jmp item_resume
client_roll:
    mov rdx,[rsp+0x208]
    mov word ptr [rax+0x12],0
    test byte ptr [rdx+0xc],128
    je client_done
    cmp byte ptr [rax],0
    je client_done
    # Save the generator's return pointer across leaf helpers.
    shl dword ptr [rax+0x18],1
    mov ecx,[rax+0x1c]
    cmp ecx,1000
    jb client_density_done
    lea ecx,[rcx+rcx-1000]
    mov r8d,65535
    cmp ecx,r8d
    cmova ecx,r8d
    mov [rax+0x1c],ecx
client_density_done:
    mov r10,rax
    movzx ecx,byte ptr [rax+2]
    mov eax,[rdx+8]
    sub rsp,0x30
    mov [rsp+0x20],r10
    call roll_xp
    mov rcx,[rsp+0x20]
    call encode_xp
    mov rax,[rsp+0x20]
    add rsp,0x30
client_done:
    lea rcx,[rsp+0x100]
    jmp client_resume
record_generation:
    movzx r9d,byte ptr [rcx+0xf]
    movzx r10d,byte ptr [rcx+0x12]
    and r10d,128
    or r9d,r10d
    mov r8d,[rip+active_seed]
    jmp record_resume
# Called with roll in RCX at the existing shared server/client penalty update.
store_levels:
    mov [rsp+8],rcx
    cmp byte ptr [rcx],0
    je store_done
    sub rsp,0x28
    call read_xp
    add rsp,0x28
    mov rcx,[rsp+8]
    lea r10,[rip+xp_table]
    movzx edx,word ptr [rcx+4]
    cmp edx,512
    jae store_done
    mov [r10+rdx*4],eax
    movzx edx,word ptr [rcx+6]
    cmp edx,512
    jae store_done
    mov [r10+rdx*4],eax
store_done:
    jmp store_resume
reset_levels:
    lea rax,[rip+xp_table]
    mov ecx,512
reset_loop:
    mov dword ptr [rax],0
    add rax,4
    dec ecx
    jnz reset_loop
    sub rsp,0xc8
    jmp reset_resume
# Original stat getter already ran: its 64-bit result is at rsp+30.
stat_bonus:
    cmp dword ptr [rsp+0xa8],85
    jne normal_stat
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
    lea r10,[rip+xp_table]
    mov edx,[r10+rax*4]
    mov rax,[rsp+0x30]
    add rax,rdx
    jmp stat_epilogue
stat_original:
    mov rax,[rsp+0x30]
    jmp stat_epilogue
normal_stat:
    cmp dword ptr [rsp+0xa8],127
    jmp stat_resume
# Existing snprintf buffer routines, only after the identified-map check.
tooltip_bonus:
    lea rcx,[rsp+0xa0]
    sub rsp,0x20
    call read_xp
    add rsp,0x20
    test eax,eax
    jz tooltip_done
    mov r9d,eax
    mov r10,[rsp+0xf8]
    mov edx,[r10+0x1c]
    sub edx,[rsp+0x40]
    cmp edx,1
    jle tooltip_done
    mov rcx,[r10+0x10]
    movsxd rax,dword ptr [rsp+0x40]
    add rcx,rax
    lea r8,[rip+tooltip_format]
    call format_text
    add [rsp+0x40],eax
tooltip_done:
    mov dword ptr [rsp+0x48],0
    jmp tooltip_resume
hud_bonus:
    mov rcx,[rsp+0x80]
    sub rsp,0x20
    call read_xp
    add rsp,0x20
    test eax,eax
    jz hud_done
    mov [rsp+0x20],eax
    movsxd rax,dword ptr [rsp+0x40]
    mov rcx,[rsp+0x90]
    add rcx,rax
    mov rdx,[rsp+0x98]
    sub rdx,rax
    cmp rdx,1
    jle hud_done
    mov r9,[rsp+0x88]
    lea r8,[rip+hud_format]
    call format_text
    add [rsp+0x40],eax
hud_done:
    mov dword ptr [rsp+0x44],0
    jmp hud_resume
.balign 8
xp_ranges:
    .long 18,59,33,90,60,162,87,199,102,226,156,268
tooltip_format: .asciz "\nExperience: +%u%%"
hud_format: .asciz "%sExperience: +%u%%"
.balign 16
xp_table:
    .zero 2048
