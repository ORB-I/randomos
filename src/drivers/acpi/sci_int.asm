[bits 64]
%include "core/irq.inc"

section .text
extern c_sci_hdlr
global sci_hdlr
sci_hdlr:
    IRQ_ENTER
    call c_sci_hdlr
    IRQ_EXIT