#include <core/std.h>
#include <core/asmh.h>
#include <core/idt.h>

#include <drivers/kbd.h>
#include <drivers/vga.h>
#include <drivers/pic.h>

static const char sc_map[128] = {
    0, 0, '1', '2', '3', '4', '5', '6', '7', '8', 
    '9', '0', '-', '=', 0, 0, 'q', 'w', 'e', 
    'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', 
    '\n', 0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 
    'k', 'l', ';', '\'', '`', 0, '\\', 'z', 'x', 
    'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, 
    '*', 0, ' '
};

static const char sc_map_shift[128] = {
    0, 0, '!', '@', '#', '$', '%', '^', '&', '*', 
    '(', ')', '_', '+', 0, 0, 'Q', 'W', 'E', 
    'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', 
    '\n', 0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 
    'K', 'L', ':', '"', '~', 0, '|', 'Z', 'X', 
    'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, 
    '*', 0, ' '
};

#define KEYBUF_SZ 256

char kbuf[KEYBUF_SZ];
u32 kb_head = 0;
u32 kb_tail = 0;
bool kb_full = false;
bool shift_pressed = false;

extern void kbd_hdlr();

void init_kbd() { init_irq(1, kbd_hdlr); }

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

bool kb_has_char(void) {
    return (kb_head != kb_tail) || kb_full;
}

char getchar(void) {
    while (1) {
        asm volatile("cli");
        if (kb_has_char()) {
            asm volatile("sti");
            break;
        }
        asm volatile(
            "sti\n\t"
            "hlt"
        );
    }
    while (!kb_has_char()) {
        asm volatile("hlt");
    }
    char c = dequeue_key();
    vga_putchar(c);
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

s32 kbd_enabled_flg = 0;
void enable_kbd() { 
    irq_enable(1);
    kbd_enabled_flg = 1;
}

void disable_kbd() {
    irq_disable(1);
    kbd_enabled_flg = 0;
}

s32 kbd_enabled() {
    return kbd_enabled_flg;
}

void c_kbd_hdlr() {
    u8 sc = inb(0x60);

    if (sc & 0x80) {
        u8 released = sc & 0x7F;
        if (released == 0x2A || released == 0x36) {
            shift_pressed = false;
        }
        return;
    }

    if (sc == 0x2A || sc == 0x36) {
        shift_pressed = true;
        return;
    }

    if (sc < 128) {
        char c = shift_pressed ? sc_map_shift[sc] : sc_map[sc];
        
        if (c) {
            enqueue_key(c);
        }
    }
}