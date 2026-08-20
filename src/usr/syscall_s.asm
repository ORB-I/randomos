[bits 64]
global syscall_s
extern syscall_c

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
