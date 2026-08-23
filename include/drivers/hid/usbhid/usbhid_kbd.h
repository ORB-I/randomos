#pragma once
#include <core/std.h>

typedef struct {
    u8 modifiers;
    u8 reserved;
    u8 keys[6];
} __attribute__((packed)) usb_hid_kbd_report_t;

int usb_hid_kbd_init();
void usb_hid_kbd_poll(u64 timeout);