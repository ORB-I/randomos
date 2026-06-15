[bits 32]
global _start

section .text
_start:
    mov eax, 4
    mov ebx, 1
    lea ecx, [rel msg]
    mov edx, 7
    int 0x80

    mov eax, 1
    mov ebx, 0
    int 0x80

section .rodata
msg: db "Hello!",10