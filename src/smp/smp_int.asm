[bits 64]
%include "core/irq.inc"

global smp_request_hdlr
extern smp_request_hdlr_c
smp_request_hdlr:
    test byte [rsp + 8], 3
    jz .smp_reqhdlr_enter
    swapgs
.smp_reqhdlr_enter:
    mov rdi, rsp
    pushaq
    cld
    call smp_request_hdlr_c
    IRQ_EXIT

global bsp_request_hdlr
extern bsp_request_hdlr_c
bsp_request_hdlr:
    IRQ_ENTER
    call bsp_request_hdlr_c
    IRQ_EXIT

global ap_stop_hdlr
ap_stop_hdlr:
    cli
.loop:
    hlt
    jmp .loop