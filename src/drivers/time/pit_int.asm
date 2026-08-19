[bits 64]
%include "core/irq.inc"

section .text
extern c_timer_hdlr
global timer_hdlr
timer_hdlr:
    IRQ_ENTER
    call c_timer_hdlr
    IRQ_EXIT