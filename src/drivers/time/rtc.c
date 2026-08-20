#include <core/asmh.h>

#include <drivers/pic.h>
#include <drivers/acpi.h>

#define REG_SECS 0x00
#define REG_MINS 0x02
#define REG_HRS  0x04
#define REG_DAY  0x07
#define REG_MON  0x08
#define REG_YEAR 0x09
#define REG_STA  0x0A
#define REG_STB  0x0B

static inline u32 rtc_bcd2bin(u8 val) {
    return ((val / 16) * 10) + (val % 16);
}

u64 rtc_gettime() {
    outb(0x70, REG_STA);
    while (inb(0x71) & 0x80);
    outb(0x70, REG_STB);
    u8 stb = inb(0x71);
    outb(0x71, stb | 0x80);

    outb(0x70, REG_SECS);
    u8 secs = inb(0x71);
    outb(0x70, REG_MINS);
    u8 mins = inb(0x71);
    outb(0x70, REG_HRS);
    u8 hrs = inb(0x71);
    outb(0x70, REG_DAY);
    u8 day = inb(0x71);
    outb(0x70, REG_MON);
    u8 mon = inb(0x71);
    outb(0x70, REG_YEAR);
    u8 hyear = inb(0x71);

    if (!(stb & 0x04)) {
        secs  = rtc_bcd2bin(secs);
        mins  = rtc_bcd2bin(mins);
        hrs   = rtc_bcd2bin(hrs);
        day   = rtc_bcd2bin(day);
        mon   = rtc_bcd2bin(mon);
        hyear = rtc_bcd2bin(hyear);
    }

    if (!(stb & 0x02) && (hrs & 0x80)) {
        hrs = ((hrs & 0x7F) + 12) % 24;
    }

    u16 year;
    u8 century_val = 0;
    if (acpi_hdl && acpi_hdl->fadt && acpi_hdl->fadt->century) {
        outb(0x70, acpi_hdl->fadt->century);
        century_val = inb(0x71);
        if (!(stb & 0x04)) {
            century_val = rtc_bcd2bin(century_val);
        }
    }

    if (century_val != 0) {
        year = (century_val * 100) + hyear;
    } else {
        year = 2000 + hyear;
    }

    u64 tdays = 0;
    for (u16 y = 1970; y < year; y++) {
        if ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0)) {
            tdays += 366;
        } else {
            tdays += 365;
        }
    }

    static const u8 dmos[] = { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    for (u8 m = 1; m < mon; m++) {
        tdays += dmos[m];
        if (m == 2 && ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0))) {
            tdays += 1;
        }
    }

    tdays += (day - 1);
    return (tdays * 86400) +
           (hrs * 3600) +
           (mins * 60) +
           secs;
}
