[bits 64]
%include "core/irq.inc"

section .text
extern scheduler_switch
extern preempt_pending
extern preempt_ctx

%define CTX_RIP       0x00
%define CTX_RSP       0x08
%define CTX_RFLAGS    0x10

%define CTX_RAX       0x18
%define CTX_RBX       0x20
%define CTX_RCX       0x28
%define CTX_RDX       0x30
%define CTX_RSI       0x38
%define CTX_RDI       0x40
%define CTX_RBP       0x48
%define CTX_R8        0x50
%define CTX_R9        0x58
%define CTX_R10       0x60
%define CTX_R11       0x68
%define CTX_R12       0x70
%define CTX_R13       0x78
%define CTX_R14       0x80
%define CTX_R15       0x88

%define CTX_CS        0x90
%define CTX_SS        0x92
%define CTX_FS        0x94
%define CTX_GS        0x96

%define CTX_FSB       0xA0
%define CTX_GSB       0xA8
%define CTX_CR3       0xB0
%define CTX_KGSB      0xC0

%define CTX_SIZE      0xC8

global preempt_hdlr
preempt_hdlr:
    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8
    push rbp
    push rdi
    push rsi
    push rdx
    push rcx
    push rbx
    push rax

    sub rsp, CTX_SIZE
    mov rdi, rsp

    mov rax, [rsp + CTX_SIZE + 0x78]
    mov [rdi + CTX_RIP], rax

    mov rax, [rsp + CTX_SIZE + 0x90]
    mov [rdi + CTX_RSP], rax

    mov rax, [rsp + CTX_SIZE + 0x88]
    mov [rdi + CTX_RFLAGS], rax

    mov ax, [rsp + CTX_SIZE + 0x80]
    mov [rdi + CTX_CS], ax

    mov ax, [rsp + CTX_SIZE + 0x98]
    mov [rdi + CTX_SS], ax

    mov rax, [rsp + CTX_SIZE + 0x00]
    mov [rdi + CTX_RAX], rax

    mov rax, [rsp + CTX_SIZE + 0x08]
    mov [rdi + CTX_RBX], rax

    mov rax, [rsp + CTX_SIZE + 0x10]
    mov [rdi + CTX_RCX], rax

    mov rax, [rsp + CTX_SIZE + 0x18]
    mov [rdi + CTX_RDX], rax

    mov rax, [rsp + CTX_SIZE + 0x20]
    mov [rdi + CTX_RSI], rax

    mov rax, [rsp + CTX_SIZE + 0x28]
    mov [rdi + CTX_RDI], rax

    mov rax, [rsp + CTX_SIZE + 0x30]
    mov [rdi + CTX_RBP], rax

    mov rax, [rsp + CTX_SIZE + 0x38]
    mov [rdi + CTX_R8], rax

    mov rax, [rsp + CTX_SIZE + 0x40]
    mov [rdi + CTX_R9], rax

    mov rax, [rsp + CTX_SIZE + 0x48]
    mov [rdi + CTX_R10], rax

    mov rax, [rsp + CTX_SIZE + 0x50]
    mov [rdi + CTX_R11], rax

    mov rax, [rsp + CTX_SIZE + 0x58]
    mov [rdi + CTX_R12], rax

    mov rax, [rsp + CTX_SIZE + 0x60]
    mov [rdi + CTX_R13], rax

    mov rax, [rsp + CTX_SIZE + 0x68]
    mov [rdi + CTX_R14], rax

    mov rax, [rsp + CTX_SIZE + 0x70]
    mov [rdi + CTX_R15], rax

    mov ax, fs
    mov [rdi + CTX_FS], ax

    mov ax, gs
    mov [rdi + CTX_GS], ax

    mov ecx, 0xC0000100
    rdmsr
    shl rdx, 32
    or rax, rdx
    mov [rdi + CTX_FSB], rax

    mov ecx, 0xC0000101
    rdmsr
    shl rdx, 32
    or rax, rdx
    mov [rdi + CTX_GSB], rax
    mov rax, cr3
    mov [rdi + CTX_CR3], rax

    mov ecx, 0xC0000102
    rdmsr
    shl rdx, 32
    or rax, rdx
    mov [rdi + CTX_KGSB], rax

    cmp word [rdi + CTX_CS], 0x08
    je .defer_preempt

    call scheduler_switch

.defer_preempt:
    mov byte [rel preempt_pending], 1
    lea rdi, [rel preempt_ctx]
    mov rsi, rsp
    mov rcx, CTX_SIZE / 8
.rep_save:
    mov rax, [rsi]
    mov [rdi], rax
    add rsi, 8
    add rdi, 8
    loop .rep_save
    add rsp, CTX_SIZE
    pop rax
    pop rbx
    pop rcx
    pop rdx
    pop rsi
    pop rdi
    pop rbp
    pop r8
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15
    iretq
