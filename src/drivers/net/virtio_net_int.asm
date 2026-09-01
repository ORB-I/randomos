[bits 64]
%include "core/irq.inc"

section .text
extern c_virtio_net_hdlr
global virtio_net_hdlr
virtio_net_hdlr:
    IRQ_ENTER
    call c_virtio_net_hdlr
    IRQ_EXIT
