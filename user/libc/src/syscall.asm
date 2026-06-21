[bits 64]
global __syscall0
global __syscall1
global __syscall2
global __syscall3
global __syscall4
global __syscall5

section .text

__syscall0:
    mov rax, [rsp + 16]
    syscall
    ret

__syscall1:
    mov rax, [rsp + 16]
    mov rbx, [rsp + 24]
    syscall
    ret

__syscall2:
    mov rax, [rsp + 16]
    mov rbx, [rsp + 24]
    mov rcx, [rsp + 32]
    syscall
    ret

__syscall3:
    mov rax, [rsp + 16]
    mov rbx, [rsp + 24]
    mov rcx, [rsp + 32]
    mov rdx, [rsp + 40]
    syscall
    ret

__syscall4:
    mov rax, [rsp + 16]
    mov rbx, [rsp + 24]
    mov rcx, [rsp + 32]
    mov rdx, [rsp + 40]
    mov rdi, [rsp + 48]
    syscall
    ret

__syscall5:
    mov rax, [rsp + 16]
    mov rbx, [rsp + 24]
    mov rcx, [rsp + 32]
    mov rdx, [rsp + 40]
    mov rdi, [rsp + 48]
    mov rsi, [rsp + 56]
    syscall
    ret