#include <core/asmh.h>
#include <core/std.h>
#include <core/panic.h>

#include <drivers/vga.h>

#define D1IOB 0x1F0
#define D1CTRLB 0x3F6

#define D2IOB 0x170
#define D2CTRLB 0x376

#define RTIO 1
#define RTCTRL 2

u8 ata_inb(u8 drv, u8 type, u32 off) {
    if (drv == 1) {
        if (type == RTIO) return inb(D1IOB + off);
        else if (type == RTCTRL) return inb(D1CTRLB + off);
        else return 0;
    } else if (drv == 2) {
        if (type == RTIO) return inb(D2IOB + off);
        else if (type == RTCTRL) return inb(D2CTRLB + off);
        else return 0;
    } else {
        return 0;
    }
}

u16 ata_inw(u8 drv, u8 type, u32 off) {
    if (drv == 1) {
        if (type == RTIO) return inw(D1IOB + off);
        else if (type == RTCTRL) return inw(D1CTRLB + off);
        else return 0;
    } else if (drv == 2) {
        if (type == RTIO) return inw(D2IOB + off);
        else if (type == RTCTRL) return inw(D2CTRLB + off);
        else return 0;
    } else {
        return 0;
    }
}

void ata_outb(u8 drv, u8 type, u32 off, u8 val) {
    if (drv == 1) {
        if (type == RTIO) outb(D1IOB + off, val);
        else if (type == RTCTRL) outb(D1CTRLB + off, val);
    } else if (drv == 2) {
        if (type == RTIO) outb(D2IOB + off, val);
        else if (type == RTCTRL) outb(D2CTRLB + off, val);
    }
}


void ata_outw(u8 drv, u8 type, u32 off, u16 val) {
    if (drv == 1) {
        if (type == RTIO) outw(D1IOB + off, val);
        else if (type == RTCTRL) outw(D1CTRLB + off, val);
    } else if (drv == 2) {
        if (type == RTIO) outw(D2IOB + off, val);
        else if (type == RTCTRL) outw(D2CTRLB + off, val);
    }
}

bool ata_identify_drive(u8 drv_id, u8 drivet) {
    ata_outb(drv_id, RTIO, 6, drivet);
    for (int i = 0; i < 4; i++) ata_inb(drv_id, RTCTRL, 0);

    ata_outb(drv_id, RTIO, 2, 0);
    ata_outb(drv_id, RTIO, 3, 0);
    ata_outb(drv_id, RTIO, 4, 0);
    ata_outb(drv_id, RTIO, 5, 0);

    ata_outb(drv_id, RTIO, 7, 0xEC);
    for (int i = 0; i < 4; i++) {
        ata_inb(drv_id, RTCTRL, 0);
    }

    u8 status = ata_inb(drv_id, RTIO, 7);
    if (status == 0) return false;

    for (int i = 0; i < 4; i++) ata_inb(drv_id, RTCTRL, 0);
    
    while (ata_inb(drv_id, RTIO, 7) & 0x80) {
    }

    status = ata_inb(drv_id, RTIO, 7);
    if (status & 0x01) return false; 
    if (!(status & 0x08)) return false;

    for (int i = 0; i < 256; i++) ata_inw(drv_id, RTIO, 0);
    return true;
}

s16 ata_find_valid_drive() {
    if (inb(D1IOB + 7) == 0xFF) {
        if (inb(D2IOB + 7) == 0xFF) {
            return -1;
        } else {
            if (ata_identify_drive(1, 0xA0)) {
                return 2;
            } else {
                return -1;
            }
        }
    } else {
        if (ata_identify_drive(1, 0xA0)) {
            return 1;
        } else {
            return -1;
        }
    }
}

// this driver is ported from my old ahh kernel so it might be shit

int ata_init() {
    s16 drv;
    if ((drv = ata_find_valid_drive()) < 0) {
        printf("ATA: No valid drive found\n");
        return -1;
    }
    printf("ATA: Found ATA drive with id %d\n", drv);
    return (u8)drv;
}

void ata_poll(u8 drv) {
    for (int i = 0; i < 4; i++) ata_inb(drv, RTCTRL, 0);
    while (ata_inb(drv, RTIO, 7) & 0x80);

    u8 stat = ata_inb(drv, RTIO, 7);
    if (stat & 0x01) panic("ATA Error during poll");
    else if (!(stat & 0x08)) panic("ATA DRQ not set");
}

void ata_secread(u8 drv, u32 lba, u8* buf) {
    ata_outb(drv, RTCTRL, 0, 0);
    ata_outb(drv, RTIO, 6, 0xE0 | ((lba >> 24) & 0x0F)); 
    for (int i = 0; i < 4; i++) ata_inb(drv, RTCTRL, 0);

    ata_outb(drv, RTIO, 2, 1);
    ata_outb(drv, RTIO, 3, lba & 0xFF);
    ata_outb(drv, RTIO, 4, (lba >> 8) & 0xFF);
    ata_outb(drv, RTIO, 5, (lba >> 16) & 0xFF);
    ata_outb(drv, RTIO, 7, 0x20);

    ata_poll(drv);

    for (int i = 0; i < 256; i++) {
        u16 dt = ata_inw(drv, RTIO, 0);
        buf[i*2] = (u8)(dt & 0xFF);
        buf[(i*2)+1] = (u8)(dt >> 8);
    }
}

void ata_secwrite(u8 drv, u32 lba, u8* buf) {
    ata_outb(drv, RTCTRL, 0, 0);

    ata_outb(drv, RTIO, 6, 0xE0 | ((lba >> 24) & 0x0F));
    ata_outb(drv, RTIO, 2, 1);
    ata_outb(drv, RTIO, 3, lba & 0xFF);
    ata_outb(drv, RTIO, 4, (lba >> 8) & 0xFF);
    ata_outb(drv, RTIO, 5, (lba >> 16) & 0xFF);

    ata_outb(drv, RTIO, 7, 0x30);

    while (!(ata_inb(drv, RTIO, 7) & 0x08));

    for (int i = 0; i < 256; i++) {
        ata_outw(drv, RTIO, 0, ((u16)buf[(i * 2) + 1] << 8) | buf[i * 2]);
    }

    ata_outb(drv, RTIO, 7, 0xE7);
    while (ata_inb(drv, RTIO, 7) & 0x80);
}