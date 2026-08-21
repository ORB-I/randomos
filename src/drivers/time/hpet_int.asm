[bits 64]
%include "core/irq.inc"

section .text
extern c_hpet_hdlr
global hpet_hdlr
hpet_hdlr:
    IRQ_ENTER
    call c_hpet_hdlr
    IRQ_EXIT
