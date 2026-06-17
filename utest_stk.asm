[bits 32]
global _start

section .text
_start:
    sub esp, 8
    mov dword [esp], 0x6C6C6548
    mov dword [esp+4], 0x000A216F

    mov eax, 3
    mov ebx, 1
    mov ecx, esp
    mov edx, 7
    int 0x80

    add esp, 8

    mov eax, 1
    mov ebx, 0
    int 0x80