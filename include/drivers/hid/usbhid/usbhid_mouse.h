#pragma once
#include <core/std.h>

typedef struct {
    u8 buttons;
    s8 x, y;
} __packed usb_hid_mouse_report_t;

int usb_hid_mouse_init();
void usb_hid_mouse_poll();