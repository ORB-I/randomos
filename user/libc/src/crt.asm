[bits 32]
global _start
extern main

section .text
_start:
    ; argc/argv already on stack
    ; bss already cleared, stack already set
    ; ty my awesome kernel
    call main
    mov ebx, eax
    mov eax, 1
    int 0x80