[bits 64]

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
%endmacro

%macro IRQ_EXIT 0
    popaq
    test byte [rsp + 8], 3
    jz %%kernint_exit
    swapgs
%%kernint_exit:
    iretq
%endmacro

section .text
extern c_kbd_hdlr
extern c_timer_hdlr
extern c_sci_hdlr

global kbd_hdlr
kbd_hdlr:
    IRQ_ENTER
    call c_kbd_hdlr
    IRQ_EXIT

global timer_hdlr
timer_hdlr:
    IRQ_ENTER
    call c_timer_hdlr
    IRQ_EXIT

global rtc_hdlr
global rtc_ticks

rtc_hdlr:
    IRQ_ENTER

    inc dword [rtc_ticks]

    mov al, 0x0C
    out 0x70, al
    in al, 0x71

    mov al, 0x20
    out 0xA0, al

    mov al, 0x20
    out 0x20, al
    
    IRQ_EXIT

section .data
align 4
rtc_ticks: dd 0

section .text
global sci_hdlr
sci_hdlr:
    IRQ_ENTER
    call c_sci_hdlr
    IRQ_EXIT