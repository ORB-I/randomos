[bits 64]

section .text
_start:
    mov rax, 3
    mov rdi, 1
    lea rsi, [msg]
    mov rdx, 6
    syscall
    
    mov rax, 1
    mov rdi, 0
    syscall

section .rodata
msg: db "Hello",10,0
