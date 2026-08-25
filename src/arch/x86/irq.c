#include <core/asmh.h>
#include <core/std.h>
#include <core/idt.h>
#include <drivers/apic.h>

#define PIC1_COMMAND 0x20
#define PIC1_DATA 0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA 0xA1
#define PIC_EOI 0x20

#define ICW1_INIT 0x10
#define ICW1_ICW4 0x01
#define ICW4_8086 0x01
#define CASCADE_IRQ 2

void pic_send_eoi(u8 irq) {
    (void)irq;
    lapic_eoi();
}

void pic_remap(s32 offset1, s32 offset2) {
    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();

    outb(PIC1_DATA, offset1);
    io_wait();
    outb(PIC2_DATA, offset2);
    io_wait();

    outb(PIC1_DATA, 1 << 2);
    io_wait();
    outb(PIC2_DATA, CASCADE_IRQ);
    io_wait();

    outb(PIC1_DATA, ICW4_8086);
    io_wait();
    outb(PIC2_DATA, ICW4_8086);
    io_wait();

    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}

void pic_disable() {
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}

void irq_disable(u8 line) {
    ioapic_mask_irq(line);
}

void irq_enable(u8 line) {
    ioapic_unmask_irq(line);
}

void init_irq(s32 irq, void (*hdlr)()) {
    idt_regintr(NULL, 0x20 + irq, hdlr, 0x8E, 1);
    ioapic_set_irq((u8)irq, (u8)(0x20 + irq), 0, true);
}