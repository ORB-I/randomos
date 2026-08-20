#include <core/std.h>
#include <core/asmh.h>
#include <core/idt.h>
#include <core/printf.h>

#include <drivers/kbd.h>
#include <drivers/term.h>
#include <drivers/apic.h>
#include <drivers/usb/usbhid_kbd.h>
#include <drivers/ps2/kbd.h>

#define KEYBUF_SZ 256

char kbuf[KEYBUF_SZ];
u32 kb_head = 0;
u32 kb_tail = 0;
bool kb_full = false;
bool shift_pressed = false;
int kb_type = 0;

u8 kbuf_sc[KEYBUF_SZ];
u32 kbsc_head = 0;
u32 kbsc_tail = 0;
bool kbsc_full = false;

u8 kbd_get_raw(void) { 
    while (!kb_has_sc()) {
        if (kb_type == KBD_USBHID) {
            usb_hid_kbd_poll();
        }
        asm volatile("pause");
    }

    u8 sc = dequeue_sc();
    return sc;
}

void init_kbd(int kbd_type) {
    kb_type = kbd_type;
    if (kb_type == KBD_PS2) {
        init_kbdps2();
        printf("KBD: Using PS/2 Keyboard\n");
    } else {
        if (usb_hid_kbd_init() < 0) {
            init_kbdps2();
            kb_type = KBD_PS2;
            printf("KBD: Using PS/2 Keyboard (USB HID Failed)\n");
        } else {
            printf("KBD: Using USB HID Keyboard\n");
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
    if (kb_full) {
        return;
    }

    kbuf[kb_head] = c;
    kb_head = (kb_head + 1) % KEYBUF_SZ;
    if (kb_head == kb_tail) {
        kb_full = true;
    }
}

char dequeue_key(void) {
    if (kb_head == kb_tail && !kb_full) {
        return 0;
    }

    char c = kbuf[kb_tail];
    kb_tail = (kb_tail + 1) % KEYBUF_SZ;
    kb_full = false;
    return c;
}

void enqueue_sc(u8 sc) {
    if (kbsc_full) {
        return;
    }
    kbuf_sc[kbsc_head] = sc;
    kbsc_head = (kbsc_head + 1) % KEYBUF_SZ;
    if (kbsc_head == kbsc_tail) {
        kbsc_full = true;
    }
}

char dequeue_sc(void) {
    if (kbsc_head == kbsc_tail && !kb_full) {
        return 0;
    }

    u8 sc = kbuf_sc[kbsc_tail];
    kbsc_tail = (kbsc_tail + 1) % KEYBUF_SZ;
    kbsc_full = false;
    return sc;
}

bool kb_has_char(void) {
    return (kb_head != kb_tail) || kb_full;
}

bool kb_has_sc(void) {
    return (kbsc_head != kbsc_tail) || kbsc_full;
}

int _kbd_noecho = 0;
void noecho(int on) {
    _kbd_noecho = on;
}

char getchar(void) {
    while (!kb_has_char()) {
        if (kb_type == KBD_USBHID) {
            if (_uhci_usbhid_kbd.ctrl && _uhci_usbhid_kbd.port != -1) {
                usb_hid_kbd_poll();
                // If no keys are in buffer after polling, reduce frequency to avoid busy polling
                if (!kb_has_char()) {
                    asm volatile("pause");
                    asm volatile("pause");
                }
            } else {
                // USB HID keyboard not connected
                asm volatile("pause");
            }
        } else {
            asm volatile("pause");
        }
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