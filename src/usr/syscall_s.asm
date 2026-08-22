[bits 64]
%include "scheduler/ctx.inc"

global syscall_s
extern syscall_c
extern scheduler_switch
extern preempt_pending

section .text
syscall_s:
    swapgs
    mov [gs:0], rsp
    mov rsp, [gs:8]

    push rcx
    push r11

    mov rcx, ds
    push rcx
    mov rcx, es
    push rcx

    mov rcx, 0x10
    mov ds, rcx
    mov es, rcx

    push r9
    push r8
    push r10
    push rdx
    push rsi
    push rdi
    push rax

    mov rdi, rsp
    call syscall_c

    cmp byte [rel preempt_pending], 0
    je .no_preempt
    mov byte [rel preempt_pending], 0

    ; build the *user* snapshot. the tick that set the flag ran in
    ; kernel CS, so its frame is useless for a later iretq
    sub rsp, CTX_SIZE
    mov rdi, rsp

    mov rax, [rsp + CTX_SIZE + 0x00]
    mov [rdi + CTX_RAX], rax
    mov rax, [rsp + CTX_SIZE + 0x08]
    mov [rdi + CTX_RDI], rax
    mov rax, [rsp + CTX_SIZE + 0x10]
    mov [rdi + CTX_RSI], rax
    mov rax, [rsp + CTX_SIZE + 0x18]
    mov [rdi + CTX_RDX], rax
    mov rax, [rsp + CTX_SIZE + 0x20]
    mov [rdi + CTX_R10], rax
    mov rax, [rsp + CTX_SIZE + 0x28]
    mov [rdi + CTX_R8], rax
    mov rax, [rsp + CTX_SIZE + 0x30]
    mov [rdi + CTX_R9], rax

    mov [rdi + CTX_RBX], rbx
    mov [rdi + CTX_RBP], rbp
    mov [rdi + CTX_R12], r12
    mov [rdi + CTX_R13], r13
    mov [rdi + CTX_R14], r14
    mov [rdi + CTX_R15], r15

    mov rax, [rsp + CTX_SIZE + 0x50]
    mov [rdi + CTX_RIP], rax
    mov rax, [rsp + CTX_SIZE + 0x48]
    mov [rdi + CTX_RFLAGS], rax

    ; gs is the kernel block; slot 0 is the user rsp stashed on entry
    mov rax, [gs:0]
    mov [rdi + CTX_RSP], rax

    mov word [rdi + CTX_CS], 0x1B
    mov word [rdi + CTX_SS], 0x23
    mov ax, fs
    mov [rdi + CTX_FS], ax
    mov word [rdi + CTX_GS], 0

    mov ecx, 0xC0000100
    rdmsr
    shl rdx, 32
    or rax, rdx
    mov [rdi + CTX_FSB], rax

    ; user gs base is sitting in KERNEL_GS_BASE while we are swapped
    mov ecx, 0xC0000102
    rdmsr
    shl rdx, 32
    or rax, rdx
    mov [rdi + CTX_GSB], rax

    cli
    call scheduler_switch
    sti

    add rsp, CTX_SIZE

.no_preempt:
    pop rax
    pop rdi
    pop rsi
    pop rdx
    pop r10
    pop r8
    pop r9

    pop rcx
    mov es, rcx
    pop rcx
    mov ds, rcx

    pop r11
    pop rcx

    push qword 0x23
    push qword [gs:0]
    push r11
    push qword 0x1B
    push rcx

    swapgs
    iretq
