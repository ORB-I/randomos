[bits 64]
global _start
extern main

section .text
_start:
    pop rdi
    mov rsi, rsp
    and rsp, -16

    call main
    
    mov rbx, rax
    mov rax, 1
    syscall