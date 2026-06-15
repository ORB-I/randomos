[BITS 32]

extern c_int_hdlr

%macro no_ec_int_hdlr 1
global int_hdlr_%1
int_hdlr_%1:
    push dword 0
    push dword %1
    jmp common_int_hdlr
%endmacro

%macro ec_int_hdlr 1
global int_hdlr_%1
int_hdlr_%1:
    push dword %1
    jmp common_int_hdlr
%endmacro

common_int_hdlr:
    pushad
    call c_int_hdlr
    popad
    add esp, 8
    iret

no_ec_int_hdlr 0    ; divide by zero
no_ec_int_hdlr 1    ; debug
no_ec_int_hdlr 2    ; NMI
no_ec_int_hdlr 3    ; breakpoint
no_ec_int_hdlr 4    ; overflow
no_ec_int_hdlr 5    ; bound range
no_ec_int_hdlr 6    ; invalid opcode
no_ec_int_hdlr 7    ; device not available
ec_int_hdlr  8      ; double fault
no_ec_int_hdlr 9    ; coprocessor overrun
ec_int_hdlr  10     ; invalid TSS
ec_int_hdlr  11     ; segment not present
ec_int_hdlr  12     ; stack fault
ec_int_hdlr  13     ; general protection fault
ec_int_hdlr  14     ; page fault
no_ec_int_hdlr 16   ; x87 FPU error
ec_int_hdlr  17     ; alignment check
no_ec_int_hdlr 18   ; machine check
no_ec_int_hdlr 19   ; SIMD FP exception


global int_hdlr_table
int_hdlr_table:
    dd int_hdlr_0
    dd int_hdlr_1
    dd int_hdlr_2
    dd int_hdlr_3
    dd int_hdlr_4
    dd int_hdlr_5
    dd int_hdlr_6
    dd int_hdlr_7
    dd int_hdlr_8
    dd int_hdlr_9
    dd int_hdlr_10
    dd int_hdlr_11
    dd int_hdlr_12
    dd int_hdlr_13
    dd int_hdlr_14
    dd 0            ; 15 is reserved, no handler
    dd int_hdlr_16
    dd int_hdlr_17
    dd int_hdlr_18
    dd int_hdlr_19