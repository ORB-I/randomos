[bits 64]
%include "core/irq.inc"

section .text
extern c_kbd_hdlr
global kbd_hdlr
kbd_hdlr:
    IRQ_ENTER
    call c_kbd_hdlr
    IRQ_EXIT