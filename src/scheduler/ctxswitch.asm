; switch_ctx(procctx_t* ctx)
[bits 64]
%include "scheduler/ctx.inc"

global switch_ctx
section .text
switch_ctx:
    cli

    mov r15, rdi

    ; selectors first: loading them resets the hidden base
    mov ax, [r15 + CTX_FS]
    mov fs, ax
    mov ax, [r15 + CTX_GS]
    mov gs, ax

    mov rax, [r15 + CTX_FSB]
    mov rdx, rax
    shr rdx, 32
    mov ecx, 0xC0000100
    wrmsr

    mov rax, [r15 + CTX_GSB]
    mov rdx, rax
    shr rdx, 32
    mov ecx, 0xC0000101
    wrmsr

    mov rax, [r15 + CTX_RFLAGS]
    and rax, ~RFLAGS_DROP
    or rax, 2

    ; 64-bit accesses ignore DS/ES, but iretq still loads SS from the
    ; frame, and user code expects the same RPL on the data segs
    movzx ecx, word [r15 + CTX_SS]
    mov ds, cx
    mov es, cx

    push rcx                         ; SS
    push qword [r15 + CTX_RSP]       ; RSP
    push rax                         ; RFLAGS

    movzx eax, word [r15 + CTX_CS]
    push rax                         ; CS
    push qword [r15 + CTX_RIP]       ; RIP

    mov rax, [r15 + CTX_RAX]
    mov rbx, [r15 + CTX_RBX]
    mov rcx, [r15 + CTX_RCX]
    mov rdx, [r15 + CTX_RDX]
    mov rsi, [r15 + CTX_RSI]
    mov rdi, [r15 + CTX_RDI]
    mov rbp, [r15 + CTX_RBP]
    mov r8,  [r15 + CTX_R8]
    mov r9,  [r15 + CTX_R9]
    mov r10, [r15 + CTX_R10]
    mov r11, [r15 + CTX_R11]
    mov r12, [r15 + CTX_R12]
    mov r13, [r15 + CTX_R13]
    mov r14, [r15 + CTX_R14]
    mov r15, [r15 + CTX_R15]
    iretq
