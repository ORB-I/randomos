global _start
extern kmain
extern load_gdt


KSTACK_SZ equ 4096
section .boot
_start:
    mov esp, kern_stack + KSTACK_SZ

    push eax
    push ebx
    call load_gdt
    pop ebx
    pop eax

    movzx ecx, word [0x040E]
    shl ecx, 4

    push ecx
    push ebx
    push eax
    call kmain
.loop:
    hlt
    jmp .loop

section .bss
global kern_stack
align 4
kern_stack:
    resb KSTACK_SZ