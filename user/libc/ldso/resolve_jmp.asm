[bits 64]
extern ldso_resvmain
global ldso_resolve
default rel

ldso_resolve:
    push r12
    push r13
    mov r12, [rsp + 16]
    mov r13, [rsp + 24]
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r14
    push r15
    mov rdi, r12
    mov rsi, r13
    call ldso_resvmain wrt ..plt
    mov r11, rax
    pop r15
    pop r14
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    pop r13
    pop r12
    add rsp, 16
    jmp r11