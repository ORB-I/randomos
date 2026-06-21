#include <core/std.h>
#include <core/asmh.h>

#include <drivers/term.h>
#include <drivers/acpi.h>
#include <drivers/pic.h>

uint64_t tick = 0;
void c_timer_hdlr() {
    tick++;
}

extern void timer_hdlr();
void pit_init(u16 frq) {
    init_irq(0, timer_hdlr);

    u16 divisor;

    if (frq == 0) {
        divisor = 0;
    } else {
        divisor = (u16)(1193182 / frq);
    }

    outb(0x43, 0x36);
    outb(0x40, divisor & 0xFF);
    outb(0x40, divisor >> 8);
}