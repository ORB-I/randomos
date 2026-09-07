[bits 64]
%include "core/irq.inc"

section .text
extern uacpi_irq_dispatch

; one stub per IRQ: the stubs are registered via init_irq() and pass
; their own IRQ number to the C dispatcher
%macro UACPI_IRQ_STUB 1
global uacpi_irq_stub_%1
uacpi_irq_stub_%1:
    IRQ_ENTER
    mov rdi, %1
    call uacpi_irq_dispatch
    IRQ_EXIT
%endmacro

UACPI_IRQ_STUB 0
UACPI_IRQ_STUB 1
UACPI_IRQ_STUB 2
UACPI_IRQ_STUB 3
UACPI_IRQ_STUB 4
UACPI_IRQ_STUB 5
UACPI_IRQ_STUB 6
UACPI_IRQ_STUB 7
UACPI_IRQ_STUB 8
UACPI_IRQ_STUB 9
UACPI_IRQ_STUB 10
UACPI_IRQ_STUB 11
UACPI_IRQ_STUB 12
UACPI_IRQ_STUB 13
UACPI_IRQ_STUB 14
UACPI_IRQ_STUB 15
UACPI_IRQ_STUB 16
UACPI_IRQ_STUB 17
UACPI_IRQ_STUB 18
UACPI_IRQ_STUB 19
UACPI_IRQ_STUB 20
UACPI_IRQ_STUB 21
UACPI_IRQ_STUB 22
UACPI_IRQ_STUB 23

section .data
global uacpi_irq_stubs
uacpi_irq_stubs:
    dq uacpi_irq_stub_0
    dq uacpi_irq_stub_1
    dq uacpi_irq_stub_2
    dq uacpi_irq_stub_3
    dq uacpi_irq_stub_4
    dq uacpi_irq_stub_5
    dq uacpi_irq_stub_6
    dq uacpi_irq_stub_7
    dq uacpi_irq_stub_8
    dq uacpi_irq_stub_9
    dq uacpi_irq_stub_10
    dq uacpi_irq_stub_11
    dq uacpi_irq_stub_12
    dq uacpi_irq_stub_13
    dq uacpi_irq_stub_14
    dq uacpi_irq_stub_15
    dq uacpi_irq_stub_16
    dq uacpi_irq_stub_17
    dq uacpi_irq_stub_18
    dq uacpi_irq_stub_19
    dq uacpi_irq_stub_20
    dq uacpi_irq_stub_21
    dq uacpi_irq_stub_22
    dq uacpi_irq_stub_23