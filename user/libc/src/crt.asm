[bits 32]
global _start
extern main

section .text
_start:
    ; argc/argv already on stack
    ; bss already cleared, stack already set
    call main
    mov ebx, eax
    mov eax, 1
    int 0x80