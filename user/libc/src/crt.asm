[bits 64]
global _start
extern _libc_setup

section .text
_start:
    pop rdi
    mov rsi, rsp

    mov rax, rdi
    inc rax
    shl rax, 3
    lea rdx, [rsi + rax]

    and rsp, -16

    call _libc_setup ; we dont call main anymore because _libc_setup wll
                     ; copy environ off the stack and call main for us

    mov rdi, rax
    mov rax, 1
    syscall