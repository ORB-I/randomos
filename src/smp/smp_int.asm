[bits 64]
%include "core/irq.inc"

global smp_request_hdlr
extern smp_request_hdlr_c
extern lapic_eoi
smp_request_hdlr:
    test byte [rsp + 8], 3
    jz .smp_reqhdlr_enter
    swapgs
.smp_reqhdlr_enter:
    mov rdi, rsp
    pushaq
    cld
    call smp_request_hdlr_c
    call lapic_eoi
    IRQ_EXIT

global bsp_request_hdlr
extern bsp_request_hdlr_c
bsp_request_hdlr:
    IRQ_ENTER
    call bsp_request_hdlr_c
    call lapic_eoi
    IRQ_EXIT

; spurious interrupts must not be EOI'd, just drop them
global lapic_spurious_hdlr
lapic_spurious_hdlr:
    iretq

global ap_stop_hdlr
ap_stop_hdlr:
    cli
.loop:
    hlt
    jmp .loop