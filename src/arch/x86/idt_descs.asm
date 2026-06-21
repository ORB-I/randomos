[BITS 64]

extern c_int_hdlr

%macro no_ec_int_hdlr 1
global int_hdlr_%1
int_hdlr_%1:
    push qword 0
    push qword %1
    jmp common_int_hdlr
%endmacro

%macro ec_int_hdlr 1
global int_hdlr_%1
int_hdlr_%1:
    push qword %1
    jmp common_int_hdlr
%endmacro

common_int_hdlr:
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

    mov rdi, rsp
    call c_int_hdlr

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

    add rsp, 16
    iretq

no_ec_int_hdlr 0
no_ec_int_hdlr 1
no_ec_int_hdlr 2
no_ec_int_hdlr 3
no_ec_int_hdlr 4
no_ec_int_hdlr 5
no_ec_int_hdlr 6
no_ec_int_hdlr 7
ec_int_hdlr  8
no_ec_int_hdlr 9
ec_int_hdlr  10
ec_int_hdlr  11
ec_int_hdlr  12
ec_int_hdlr  13
ec_int_hdlr  14
no_ec_int_hdlr 16
ec_int_hdlr  17
no_ec_int_hdlr 18
no_ec_int_hdlr 19

global int_hdlr_table
int_hdlr_table:
    dq int_hdlr_0
    dq int_hdlr_1
    dq int_hdlr_2
    dq int_hdlr_3
    dq int_hdlr_4
    dq int_hdlr_5
    dq int_hdlr_6
    dq int_hdlr_7
    dq int_hdlr_8
    dq int_hdlr_9
    dq int_hdlr_10
    dq int_hdlr_11
    dq int_hdlr_12
    dq int_hdlr_13
    dq int_hdlr_14
    dq 0            
    dq int_hdlr_16
    dq int_hdlr_17
    dq int_hdlr_18
    dq int_hdlr_19