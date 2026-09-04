[bits 64]
%include "core/irq.inc"

section .text
extern c_virtio_input_hdlr
global virtio_input_hdlr
virtio_input_hdlr:
    IRQ_ENTER
    call c_virtio_input_hdlr
    IRQ_EXIT