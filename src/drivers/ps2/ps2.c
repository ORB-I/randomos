#include <core/asmh.h>

void ps2_wait_write() {
    while (inb(0x64) & 2);
}

void ps2_wait_read() {
    while (!(inb(0x64) & 1));
}

u8 ps2_dataread() {
    return inb(0x60);
}

void ps2_datawrite(u8 data) {
    outb(0x60, data);
}

u8 ps2_statread() {
    return inb(0x64);
}

void ps2_cmdwrite(u8 cmd) {
    outb(0x64, cmd);
}