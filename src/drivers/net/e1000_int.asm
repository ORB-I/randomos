[bits 64]
%include "core/irq.inc"

section .text
extern c_e1000_hdlr
global e1000_hdlr
e1000_hdlr:
    IRQ_ENTER
    call c_e1000_hdlr
    IRQ_EXIT
