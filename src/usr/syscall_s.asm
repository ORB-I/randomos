[bits 64]
global syscall_s
extern syscall_c

section .text
syscall_s:
    swapgs
    mov [gs:0], rsp
    mov rsp, [gs:8]

    push r11
    push rcx

    push r9
    push r8
    push r10
    push rdx
    push rsi
    push rdi
    push rax

    call syscall_c

    pop rax
    pop rdi
    pop rsi
    pop rdx
    pop r10
    pop r8
    pop r9
    
    pop rcx
    pop r11

    mov rsp, [gs:0]
    swapgs
    sysret