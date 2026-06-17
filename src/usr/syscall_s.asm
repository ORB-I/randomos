[bits 32]
global syscall_s
extern syscall_c

section .text
syscall_s:
    pushad

    mov ax, 0x10            
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp
    call syscall_c
    add esp, 4

    popad
    iret