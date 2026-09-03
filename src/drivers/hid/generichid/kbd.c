#include <core/std.h>
#include <core/asmh.h>
#include <core/idt.h>
#include <core/kprint.h>
#include <core/kqueue.h>

#include <drivers/hid/kbd.h>
#include <drivers/display/term.h>
#include <drivers/apic.h>
#include <drivers/hid/usbhid/usbhid_kbd.h>
#include <drivers/hid/ps2/kbd.h>
#include <drivers/time/clock.h>

kqueue_t* kbchq = NULL;
kqueue_t* kbscq = NULL;

static bool shift_pressed = false;
static int kb_type = 0;

u8 kbd_get_raw(void) {
    while (!kb_has_sc()) {
        if (kb_type == KBD_USBHID) {
            usb_hid_kbd_poll(10);
        }
        asm volatile("pause");
    }

    u8 sc = dequeue_sc();
    return sc;
}

u8 kbd_getrawto(u64 timeout) {
    if (kb_type == KBD_USBHID) {
        usb_hid_kbd_poll(timeout);
    } else {
        u64 c = 0;
        while (!kb_has_sc() && c < timeout) {
            sleepms(1);
            c++;
        }
    }

    if (!kb_has_sc()) return 0;

    u8 sc = dequeue_sc();
    return sc;
}

void init_kbd(int kbd_type) {
    kb_type = kbd_type;
    kbchq = kqueue_init(512);
    if (!kbchq) {
        kprint("Could not create character queue\n");
        return;
    }

    kbscq = kqueue_init(512);
    if (!kbscq) {
        kprint("Could not create scancode queue\n");
        return;
    }

    if (kb_type == KBD_PS2) {
        init_kbdps2();
        kprint("KBD: Using PS/2 Keyboard\n");
    } else {
        if (usb_hid_kbd_init() < 0) {
            init_kbdps2();
            kb_type = KBD_PS2;
            kprint("KBD: Using PS/2 Keyboard (USB HID Failed)\n");
        } else {
            kprint("KBD: Using USB HID Keyboard\n");
        }
    }
}

void kbd_setshift(int shift) {
    shift_pressed = shift;
}

int kbd_getshift() {
    return shift_pressed;
}

void enqueue_key(char c) {
    kqueue_enqueue(kbchq, (u8*)&c, 1);
}

char dequeue_key(void) {
    char c;
    if (kqueue_dequeue(kbchq, (u8*)&c, 1) == 0) {
        return 0;
    }
    return c;
}

void enqueue_sc(u8 sc) {
    kqueue_enqueue(kbscq, &sc, 1);
}

char dequeue_sc(void) {
    u8 sc;
    if (kqueue_dequeue(kbscq, &sc, 1) == 0) {
        return 0;
    }
    return sc;
}

bool kb_has_char(void) {
    return kqueue_queued(kbchq) > 0;
}

bool kb_has_sc(void) {
    return kqueue_queued(kbscq) > 0;
}

int _kbd_noecho = 0;
void noecho(int on) {
    _kbd_noecho = on;
}

char getchar(void) {
    while (!kb_has_char()) {
        if (kb_type == KBD_USBHID) {
            usb_hid_kbd_poll(10);
        }
        asm volatile("pause");
    }

    char c = dequeue_key();
    if (!_kbd_noecho) {
        term_putchar(c);
    }
    return c;
}

usize getstr(char* buf, usize ntoread) {
    usize nread = 0;
    for (usize i = 0; i < ntoread; i++) {
        char c = getchar();
        if (c == '\n') {
            return nread;
        }
        buf[i] = c;
        nread++;
    }
    return nread;
}
