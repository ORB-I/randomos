#include <core/asmh.h>
#include <drivers/time/clock.h>

void ps2_wait_write() {
    while (inb(0x64) & 2);
}

void ps2_wait_read() {
    while (!(inb(0x64) & 1));
}

u8 ps2_dataread() {
    return inb(0x60);
}

u8 ps2_datareadto(u16 to) {
    while (to > 0) {
        u8 status = ps2_dataread();
        if (status & 0x01) {
            return status;
        }

        sleepms(1);
        to--;
    }

    return ps2_dataread();
}

void ps2_datawrite(u8 data) {
    outb(0x60, data);
}

u8 ps2_statread() {
    return inb(0x64);
}

u8 ps2_statreadto(u16 to) {
    while (to > 0) {
        u8 status = ps2_statread();
        if (status & 0x01) {
            return status;
        }

        sleepms(1);
        to--;
    }

    return ps2_statread();
}

void ps2_cmdwrite(u8 cmd) {
    outb(0x64, cmd);
}

int ps2_ackcmd(u8 cmd) {
    ps2_wait_write();
    ps2_datawrite(cmd);
    ps2_wait_read();
    return (ps2_dataread() == 0xFA);
}

int isps2dc() {
    while (inb(0x64) & 1) inb(0x60);

    ps2_wait_write();
    ps2_cmdwrite(0xAD);
    ps2_wait_write();
    ps2_cmdwrite(0xA7);

    ps2_wait_write();
    ps2_cmdwrite(0x20);

    ps2_wait_read();
    uint8_t cfg = ps2_dataread();

    ps2_wait_write();
    ps2_cmdwrite(0xA8);
    ps2_wait_write();
    ps2_cmdwrite(0x20);

    ps2_wait_read();
    uint8_t pecfg = ps2_dataread();
    if ((cfg & (1 << 5)) && !(pecfg & (1 << 5))) {
        ps2_wait_write();
        ps2_cmdwrite(0xA9);
        ps2_wait_read();
        uint8_t res = ps2_dataread();
        if (res != 0xFF) {
            ps2_wait_write();
            ps2_cmdwrite(0xA7);
            ps2_wait_write();
            ps2_cmdwrite(0xAE);

            return 1;
        }
    }
    ps2_cmdwrite(0xAE);
    return 0;
}
