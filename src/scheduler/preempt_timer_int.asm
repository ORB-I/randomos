[bits 64]
%include "core/irq.inc"
%include "scheduler/ctx.inc"

section .text
extern scheduler_switch
extern preempt_pending
extern lapic_eoi

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

    ; leave CTX_CR3 alone — process_state_t keeps the HHDM pointer
    ; from spawn, and cr3 the register is physical

    mov rbx, rdi
    and rsp, ~0xF
    call lapic_eoi
    mov rdi, rbx

    ; still in the kernel (syscall, nested irq, loader, …): just mark
    ; it and let syscall_s switch using the *user* return state
    test byte [rdi + CTX_CS], 3
    jz .defer_preempt

    call scheduler_switch

    ; only one runnable process — drop back into it
    jmp .leave

.defer_preempt:
    mov byte [rel preempt_pending], 1

.leave:
    ; rdi still holds the ctx we allocated; the original rsp is ctx + SIZE
    lea rsp, [rdi + CTX_SIZE]
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
