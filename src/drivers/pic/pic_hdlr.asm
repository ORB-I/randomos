[BITS 32]

section .text
extern c_kbd_hdlr
extern c_timer_hdlr
extern c_sci_hdlr

global kbd_hdlr
kbd_hdlr:
    pushad

    call c_kbd_hdlr

    mov al, 0x20
    out 0x20, al

    popad
    iret

global timer_hdlr
timer_hdlr:
    pushad
    call c_timer_hdlr

    mov al, 0x20
    out 0x20, al

    popad
    iret

global rtc_hdlr
global rtc_ticks

rtc_hdlr:
    pushad

    inc dword [rtc_ticks]

    mov al, 0x0C
    out 0x70, al
    in al, 0x71

    mov al, 0x20
    out 0xA0, al

    mov al, 0x20
    out 0x20, al
    popad
    iret

section .data
align 4
rtc_ticks: dd 0

section .text
global sci_hdlr
sci_hdlr:
    pushad

    call c_sci_hdlr

    mov al, 0x20
    out 0xA0, al

    mov al, 0x20
    out 0x20, al
    popad
    iret