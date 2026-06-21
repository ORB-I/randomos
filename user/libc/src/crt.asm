[bits 64]
global _start
extern main

section .text
_start:
    call main
    mov rbx, rax
    mov rax, 1
    syscall