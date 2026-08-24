[bits 64]
%include "core/irq.inc"
%include "scheduler/ctx.inc"

section .text
extern scheduler_switch
extern preempt_pending
extern lapic_eoi

global preempt_hdlr
preempt_hdlr:
    pushaq

    sub rsp, CTX_SIZE
    mov rdi, rsp

    ; GPRs pushed by pushaq on stack at [rsp + CTX_SIZE + ...]:
    ; [rsp + CTX_SIZE + 0x00] = r15
    ; [rsp + CTX_SIZE + 0x08] = r14
    ; [rsp + CTX_SIZE + 0x10] = r13
    ; [rsp + CTX_SIZE + 0x18] = r12
    ; [rsp + CTX_SIZE + 0x20] = r11
    ; [rsp + CTX_SIZE + 0x28] = r10
    ; [rsp + CTX_SIZE + 0x30] = r9
    ; [rsp + CTX_SIZE + 0x38] = r8
    ; [rsp + CTX_SIZE + 0x40] = rbp
    ; [rsp + CTX_SIZE + 0x48] = rdi
    ; [rsp + CTX_SIZE + 0x50] = rsi
    ; [rsp + CTX_SIZE + 0x58] = rdx
    ; [rsp + CTX_SIZE + 0x60] = rcx
    ; [rsp + CTX_SIZE + 0x68] = rbx
    ; [rsp + CTX_SIZE + 0x70] = rax
    ; CPU interrupt frame:
    ; [rsp + CTX_SIZE + 0x78] = RIP
    ; [rsp + CTX_SIZE + 0x80] = CS
    ; [rsp + CTX_SIZE + 0x88] = RFLAGS
    ; [rsp + CTX_SIZE + 0x90] = RSP
    ; [rsp + CTX_SIZE + 0x98] = SS

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

    mov rax, [rsp + CTX_SIZE + 0x70]
    mov [rdi + CTX_RAX], rax

    mov rax, [rsp + CTX_SIZE + 0x68]
    mov [rdi + CTX_RBX], rax

    mov rax, [rsp + CTX_SIZE + 0x60]
    mov [rdi + CTX_RCX], rax

    mov rax, [rsp + CTX_SIZE + 0x58]
    mov [rdi + CTX_RDX], rax

    mov rax, [rsp + CTX_SIZE + 0x50]
    mov [rdi + CTX_RSI], rax

    mov rax, [rsp + CTX_SIZE + 0x48]
    mov [rdi + CTX_RDI], rax

    mov rax, [rsp + CTX_SIZE + 0x40]
    mov [rdi + CTX_RBP], rax

    mov rax, [rsp + CTX_SIZE + 0x38]
    mov [rdi + CTX_R8], rax

    mov rax, [rsp + CTX_SIZE + 0x30]
    mov [rdi + CTX_R9], rax

    mov rax, [rsp + CTX_SIZE + 0x28]
    mov [rdi + CTX_R10], rax

    mov rax, [rsp + CTX_SIZE + 0x20]
    mov [rdi + CTX_R11], rax

    mov rax, [rsp + CTX_SIZE + 0x18]
    mov [rdi + CTX_R12], rax

    mov rax, [rsp + CTX_SIZE + 0x10]
    mov [rdi + CTX_R13], rax

    mov rax, [rsp + CTX_SIZE + 0x08]
    mov [rdi + CTX_R14], rax

    mov rax, [rsp + CTX_SIZE + 0x00]
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

    mov r12, rdi
    and rsp, ~0xF
    call lapic_eoi
    mov rdi, r12

    ; still in the kernel (syscall, nested irq, loader, …): just mark
    ; it and let syscall_s switch using the *user* return state
    test byte [r12 + CTX_CS], 3
    jz .defer_preempt

    call scheduler_switch

    ; only one runnable process — drop back into it
    jmp .leave

.defer_preempt:
    mov byte [rel preempt_pending], 1

.leave:
    lea rsp, [r12 + CTX_SIZE]
    popaq
    iretq
