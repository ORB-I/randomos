#include <core/errno.h>
#include <core/std.h>
#include <core/asmh.h>
#include <core/idt.h>
#include <core/kqueue.h>
#include <drivers/apic.h>
#include <lib/string.h>
#include <drivers/hid/ps2/mouse.h>
#include <core/kprint.h>
#include <drivers/hid/usbhid/usbhid_mouse.h>
#include <drivers/hid/virtio_input.h>
#include <drivers/hid/ps2/ps2.h>
#include <drivers/hid/mouse.h>
#include <drivers/time/clock.h>

kqueue_t* msq = NULL;
int mb_type = 0;

int mouse_has_info() {
    return (kqueue_queued(msq) / sizeof(mouse_info_t)) > 0;
}

int get_mouse_info(mouse_info_t* buf) {
    if (mb_type == 0) return -EINVAL;
    while (!mouse_has_info()) {
        if (mb_type == MOUSE_USBHID) {
            usb_hid_mouse_poll();
        } else if (mb_type == MOUSE_VIRTIO) {
            virtio_input_poll();
        }
        asm volatile("pause");
        sleepms(5);
    }

    mouse_info_t info = dequeue_mouse();
    memcpy(buf, &info, sizeof(info));
    return 0;
}

void enqueue_mouse(mouse_info_t info) {
    kqueue_enqueue(msq, (u8*)&info, sizeof(mouse_info_t));
}

mouse_info_t dequeue_mouse() {
    mouse_info_t msinfo = {0, 0, 0};
    if (kqueue_dequeue(msq, (u8*)&msinfo, sizeof(mouse_info_t)) < sizeof(mouse_info_t)) {
        return (mouse_info_t){0, 0, 0};
    }
    return msinfo;
}

int init_mouse(int type) {
    msq = kqueue_init(sizeof(mouse_info_t) * 256);
    if (!msq) {
        kprint("Failed to create queue\n");
        return -ENOMEM;
    }

    mb_type = type;
    if (type == MOUSE_VIRTIO) {
        kprint("MOUSE: Using VirtIO Input Mouse\n");
        return 0;
    } else if (type == MOUSE_PS2) {
        if (!isps2dc()) {
            kprint("MOUSE: No mouse available\n");
            return -ENOEXIST;
        }
        if (!has_ps2mouse()) {
            kprint("MOUSE: No mouse available\n");
            return -ENOEXIST;
        }
        init_mouseps2();
        kprint("MOUSE: Using PS/2 Mouse\n");
        return 0;
    } else {
        if (usb_hid_mouse_init() < 0) {
            if (!isps2dc()) {
                kprint("MOUSE: No mouse available\n");
                return -ENOEXIST;
            }
            if (!has_ps2mouse()) {
                kprint("MOUSE: No mouse available\n");
                return -ENOEXIST;
            }
            init_mouseps2();
            mb_type = MOUSE_PS2;
            kprint("MOUSE: Using PS/2 Mouse (USB HID Failed)\n");
            return 0;
        } else {
            kprint("MOUSE: Using USB HID Mouse\n");
            return 0;
        }
    }
}
