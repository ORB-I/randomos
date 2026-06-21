[bits 64]
global syscall_s
extern syscall_c

section .text
syscall_s:
    swapgs
    mov [gs:0], rsp
    mov rsp, [gs:8]

    push r11
    push r10
    push r9
    push r8
    push rdi
    push rsi

    push rdx
    push rcx
    push rbx
    push rax

    mov rdi, rsp
    call syscall_c

    pop rax
    pop rbx
    pop rcx
    pop rdx

    pop rsi
    pop rdi
    pop r8
    pop r9
    pop r10
    pop r11

    mov rsp, [gs:0]
    swapgs
    sysret