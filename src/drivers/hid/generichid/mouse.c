#include <core/std.h>
#include <core/asmh.h>
#include <core/idt.h>
#include <drivers/apic.h>
#include <lib/string.h>
#include <drivers/hid/ps2/mouse.h>
#include <core/printf.h>
#include <drivers/hid/usbhid/usbhid_mouse.h>
#include <drivers/hid/ps2/ps2.h>
#include <drivers/hid/mouse.h>
#include <drivers/time/clock.h>

#define MOUSEBUF_SZ 256
mouse_info_t mousebuf[MOUSEBUF_SZ];
u32 mb_head = 0;
u32 mb_tail = 0;
bool mb_full = 0;
int mb_type = 0;

int mouse_has_info() {
    return (mb_head != mb_tail) || mb_full;
}

int get_mouse_info(mouse_info_t* buf) {
    if (mb_type == 0) return -1;
    while (!mouse_has_info()) {
        if (mb_type == MOUSE_USBHID) {
            usb_hid_mouse_poll();
        }
        asm volatile("pause");
        sleepms(5);
    }

    mouse_info_t info = dequeue_mouse();
    memcpy(buf, &info, sizeof(info));
    return 0;
}

void enqueue_mouse(mouse_info_t info) {
    if (!mb_full) {
        memcpy(&mousebuf[mb_head], &info, sizeof(info));

        mb_head = (mb_head + 1) % MOUSEBUF_SZ;
        if (mb_head == mb_tail) {
            mb_full = true;
        }
    }
}

mouse_info_t dequeue_mouse() {
    if (mb_head == mb_tail && !mb_full) {
        return (mouse_info_t){0,0,0};
    }

    mouse_info_t info = mousebuf[mb_tail];
    mb_tail = (mb_tail + 1) % MOUSEBUF_SZ;
    mb_full = false;
    return info;
}

int init_mouse(int type) {
    mb_type = type;
    if (type == MOUSE_PS2) {
        if (!isps2dc()) {
            printf("MOUSE: No mouse available\n");
            return -1;
        }
        if (!has_ps2mouse()) {
            printf("MOUSE: No mouse available\n");
            return -1;
        }
        init_mouseps2();
        printf("MOUSE: Using PS/2 Mouse\n");
        return 0;
    } else {
        if (usb_hid_mouse_init() < 0) {
            if (!isps2dc()) {
                printf("MOUSE: No mouse available\n");
                return -1;
            }
            if (!has_ps2mouse()) {
                printf("MOUSE: No mouse available\n");
                return -1;
            }
            init_mouseps2();
            mb_type = MOUSE_PS2;
            printf("MOUSE: Using PS/2 Mouse (USB HID Failed)\n");
            return 0;
        } else {
            printf("MOUSE: Using USB HID Mouse\n");
            return 0;
        }
    }
}
