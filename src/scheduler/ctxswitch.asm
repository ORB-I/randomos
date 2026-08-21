global switch_ctx
section .text
switch_ctx:
    cli

    mov r15, rdi
    mov rax, [r15 + 0x18]
    mov rbx, [r15 + 0x20]
    mov rcx, [r15 + 0x28]
    mov rdx, [r15 + 0x30]
    mov rsi, [r15 + 0x38]
    mov rdi, [r15 + 0x40]
    mov rbp, [r15 + 0x48]
    mov r8,  [r15 + 0x58]
    mov r10, [r15 + 0x60]
    mov r11, [r15 + 0x68]
    mov r12, [r15 + 0x70]
    mov r13, [r15 + 0x78]
    mov r14, [r15 + 0x80]

    movzx rax, word [r15 + 0x92]
    push rax

    push qword [r15 + 0x08]
    push qword [r15 + 0x10]

    movzx rax, word [r15 + 0x90]
    push rax

    push qword [r15 + 0x00]
    mov r15, [r15 + 0x88]

    iretq
