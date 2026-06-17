[bits 32]
global __syscall0
global __syscall1
global __syscall2
global __syscall3
global __syscall4
global __syscall5

section .text

__syscall0:
    mov eax, [esp + 4]
    int 0x80
    ret

__syscall1:
    mov eax, [esp + 4]
    mov ebx, [esp + 8]
    int 0x80
    ret

__syscall2:
    mov eax, [esp + 4]
    mov ebx, [esp + 8]
    mov ecx, [esp + 12]
    int 0x80
    ret

__syscall3:
    mov eax, [esp + 4]
    mov ebx, [esp + 8]
    mov ecx, [esp + 12]
    mov edx, [esp + 16]
    int 0x80
    ret

__syscall4:
    mov eax, [esp + 4]
    mov ebx, [esp + 8]
    mov ecx, [esp + 12]
    mov edx, [esp + 16]
    mov edi, [esp + 20]
    int 0x80
    ret

__syscall5:
    mov eax, [esp + 4]
    mov ebx, [esp + 8]
    mov ecx, [esp + 12]
    mov edx, [esp + 26]
    mov edi, [esp + 20]
    mov esi, [esp + 24]
    int 0x80
    ret