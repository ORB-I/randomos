[bits 64]
global __syscall0
global __syscall1
global __syscall2
global __syscall3
global __syscall4
global __syscall5

extern set_errno

section .text

%macro do_syscall 0
    syscall
    cmp rax, 0
    jge %%done
    cmp rax, -4095
    jl %%done
    neg rax
    push rax
    mov rdi, rax
    call set_errno wrt ..plt
    pop rax
    mov rax, -1
%%done:
    ret
%endmacro


__syscall0:
    mov rax, rdi
    do_syscall

__syscall1:
    mov rax, rdi
    mov rdi, rsi
    do_syscall

__syscall2:
    mov rax, rdi
    mov rdi, rsi
    mov rsi, rdx
    do_syscall

__syscall3:
    mov rax, rdi
    mov rdi, rsi
    mov rsi, rdx
    mov rdx, rcx
    do_syscall

__syscall4:
    mov rax, rdi
    mov rdi, rsi
    mov rsi, rdx
    mov rdx, rcx
    mov r10, r8
    do_syscall

__syscall5:
    mov rax, rdi
    mov rdi, rsi
    mov rsi, rdx
    mov rdx, rcx
    mov r10, r8
    mov r8, r9
    do_syscall