[bits 64]
%include "core/irq.inc"

section .text
extern c_mouse_hdlr
global mouse_hdlr
mouse_hdlr:
    IRQ_ENTER
    call c_mouse_hdlr
    IRQ_EXIT