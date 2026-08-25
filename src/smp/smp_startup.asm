[bits 16]

section .smp_startup
smp_entry16:
    cli
    cld
    xor ax, ax
    mov ds, ax

    mov bx, (0x8000 + (.temp_gdtr16 - smp_entry16))
    lgdt [bx]
    mov eax, cr0
    or al, 1
    mov cr0, eax
[bits 32]
    jmp 0x08:(0x8000 + (.smp_entry32 - smp_entry16))

.smp_entry32:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax

    mov eax, cr4
    or eax, 1<<5
    mov cr4, eax

    mov ebx, (0x8000 + (.local_cr3 - smp_entry16))
    mov eax, [ebx]
    mov cr3, eax

    mov ecx, 0xC0000080
    rdmsr
    or eax, 1<<8
    wrmsr

    mov eax, cr0
    or eax, 1<<31
    mov cr0, eax
[bits 64]
    mov rax, smp_entry64
    jmp rax

align 16
.temp_gdt16:
    dq 0
    dw 0xFFFF, 0x0000, 0x009A, 0xCF00
    dw 0xFFFF, 0x0000, 0x0092, 0xCF00
.temp_gdt16_end:

.temp_gdtr16:
    dw (.temp_gdt16_end - .temp_gdt16) - 1
    dd (0x8000 + (.temp_gdt16 - smp_entry16))

align 8
.local_cr3:         dd 0       
.local_stacks_lst:  dq 0       
.local_stacks_cnt:  dq 0    

[bits 64]
extern smp_main

global __smp_startup_cr3
global __smp_stacks_lst
global __smp_stacks_lstn

__smp_startup_cr3   equ .local_cr3
__smp_stacks_lst    equ .local_stacks_lst
__smp_stacks_lstn   equ .local_stacks_cnt

smp_entry64:
    mov eax, 1
    cpuid
    shr ebx, 24
    movzx rbx, bl

    mov rsi, [__smp_stacks_lst]
    mov rcx, [__smp_stacks_lstn]

    mov r8, 16392
.find_stk_lop:
    test rcx, rcx
    jz .stk_not_found

    cmp rbx, [rsi]
    je .stk_found

    add rsi, r8
    dec rcx
    jmp .find_stk_lop
.stk_found:
    add rsi, 16392
    mov rsp, rsi

    and rsp, -16
    mov rdi, rbx
    call smp_main
.stk_not_found:
    cli
.hlt_loop:
    hlt
    jmp .hlt_loop