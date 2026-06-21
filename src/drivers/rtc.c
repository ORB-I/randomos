#include <core/asmh.h>

#include <drivers/pic.h>

extern volatile u32 rtc_ticks;
s32 rtc_enabled_flg = 0;
extern void rtc_hdlr();

void init_rtc() {
    init_irq(0x8, rtc_hdlr);

    rtc_ticks = 0;

    u8 prev;

    outb(0x70, 0x8B);
    prev = inb(0x71);
    outb(0x70, 0x8B);
    outb(0x71, prev | 0x40);

    outb(0x70, 0x8A);
    prev = inb(0x71);
    outb(0x70, 0x8A);
    outb(0x71, (prev & 0xF0) | 0x06);
}

void enable_rtc() {
    irq_enable(8);
    irq_enable(2);
    rtc_enabled_flg = 1;
}

void disable_rtc() {
    irq_disable(2);
    irq_disable(8);
    rtc_enabled_flg = 0;
}

void reset_rtc() {
    s32 reenable = 0;
    if (rtc_enabled_flg) { reenable = 1; disable_rtc(); }
    rtc_ticks = 0;
    if (reenable) enable_rtc();
}

s32 rtc_enabled() {
    return rtc_enabled_flg;
}

u32 rtc_getticks() {
    return rtc_ticks;
}

void rtc_sleep(s32 secs) {
    reset_rtc();
    u32 tgt = rtc_ticks + (secs * 1024);
    while (rtc_getticks() < tgt) asm volatile("hlt");
}