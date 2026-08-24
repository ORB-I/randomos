%macro pushaq 0
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
%endmacro

%macro popaq 0
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
%endmacro

%macro IRQ_ENTER 0
    test byte [rsp + 8], 3
    jz %%kernint_enter
    swapgs
%%kernint_enter:
    pushaq
    cld
    mov rax, rsp
    and rsp, -16
    push rax
%endmacro

%macro IRQ_EXIT 0
    pop rsp
    popaq
    test byte [rsp + 8], 3
    jz %%kernint_exit
    swapgs
%%kernint_exit:
    iretq
%endmacro