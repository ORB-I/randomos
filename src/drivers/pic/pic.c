#include <core/asmh.h>
#include <core/std.h>
#include <core/idt.h>

#define PIC1_BASE 0x20
#define PIC2_BASE 0xA0
#define PIC1_COMMAND PIC1_BASE
#define PIC1_DATA (PIC1_BASE + 1)
#define PIC2_COMMAND PIC2_BASE
#define PIC2_DATA (PIC2_BASE + 1)
#define PIC_EOI 0x20

#define ICW1_ICW4 0x01
#define ICW1_SINGLE 0x02
#define ICW1_INTERVAL4 0x04
#define ICW1_LEVEL 0x08
#define ICW1_INIT 0x10

#define ICW4_8086 0x01
#define ICW4_AUTO 0x02
#define ICW4_BUF_SLAVE 0x08
#define ICW4_BUF_MASTER 0x0C
#define ICW4_SFNM 0x10

#define IA32_APIC_BASE_MSR 0x1B

void fuck_off_apic_you_interrupt_stealing_bitch() {
    u64 apic_base = rdmsr(IA32_APIC_BASE_MSR);
    apic_base &= ~(1 << 11);
    wrmsr(IA32_APIC_BASE_MSR, apic_base);
}

#define CASCADE_IRQ 2

void pic_send_eoi(u8 irq) {
    if (irq >= 8) {
        outb(PIC2_COMMAND, PIC_EOI);
    }
    outb(PIC1_COMMAND, PIC_EOI);
}

void pic_remap(s32 offset1, s32 offset2) {
    fuck_off_apic_you_interrupt_stealing_bitch();
    u8 mask1 = inb(PIC1_DATA);
    u8 mask2 = inb(PIC2_DATA);

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

    outb(PIC1_DATA, mask1);
    outb(PIC2_DATA, mask2);
}

void pic_disable() {
    outb(PIC1_DATA, 0xff);
    outb(PIC2_DATA, 0xff);
}

void irq_disable(u8 line) {
    u16 port;
    u8 value;

    if (line < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        line -= 8;
    }
    value = inb(port) | (1 << line);
    outb(port, value);
}

void irq_enable(u8 line) {
    u16 port;
    u8 value;

    if (line < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        line -= 8;
    }
    value = inb(port) & ~(1 << line);
    outb(port, value);
}

void init_irq(s32 irq, void (*hdlr)()) {
    idt_regintr(0x20 + irq, hdlr, 0x8E);
}