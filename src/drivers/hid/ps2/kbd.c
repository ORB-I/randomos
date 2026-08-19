#include <drivers/ps2/ps2.h>
#include <drivers/apic.h>
#include <core/asmh.h>
#include <drivers/kbd.h>
#include <core/idt.h>

static const char sc_map[128] = {
    0, 0, '1', '2', '3', '4', '5', '6', '7', '8', 
    '9', '0', '-', '=', '\b', '\t', 'q', 'w', 'e', 
    'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', 
    '\n', 0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 
    'k', 'l', ';', '\'', '`', 0, '\\', 'z', 'x', 
    'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, 
    '*', 0, ' '
};

static const char sc_map_shift[128] = {
    0, 0, '!', '@', '#', '$', '%', '^', '&', '*', 
    '(', ')', '_', '+', 0, 0, 'Q', 'W', 'E', 
    'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', 
    '\n', 0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 
    'K', 'L', ':', '"', '~', 0, '|', 'Z', 'X', 
    'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, 
    '*', 0, ' '
};

extern void kbd_hdlr();

void init_kbdps2() {
    ps2_wait_write();
    ps2_cmdwrite(0xAE);
    ps2_wait_write();
    ps2_cmdwrite(0x20);
    ps2_wait_read();

    u8 cb = ps2_dataread();
    cb |= 0x01; 

    ps2_cmdwrite(0x60);
    ps2_wait_write();
    ps2_datawrite(cb);

    idt_regintr(0x21, kbd_hdlr, 0x8E, 1);
    ioapic_set_irq(1, 0x21, get_lapic_id(), 0);
    ioapic_unmask_irq(1);
}

void c_kbd_hdlr() {
    u8 sc = inb(0x60);
    enqueue_sc(sc);

    if (sc & 0x80) {
        u8 released = sc & 0x7F;
        if (released == 0x2A || released == 0x36) {
            kbd_setshift(false);
        }
        lapic_eoi();
        return;
    }

    if (sc == 0x2A || sc == 0x36) {
        kbd_setshift(true);
        lapic_eoi();
        return;
    }

    if (sc < 128) {
        char c = kbd_getshift() ? sc_map_shift[sc] : sc_map[sc];
        if (c) {
            enqueue_key(c);
        }
    }
    lapic_eoi();
}