#include <core/std.h>
#include <core/asmh.h>
#include <core/limreqs.h>
#include <lib/printf.h>
#include <drivers/vga.h>
#include <core/mem/vmm.h>

static volatile u16* vga;
static vga_color_t current_color = VGA_WHITE;
static volatile usize vga_idx = 0;
static vga_color_t default_color = VGA_WHITE;

void vga_init() {
    vga = (volatile u16*)(HHDM_START + 0xB8000);
}

static u16 pack_vga_char(char c, vga_color_t color) {
    return ((u16)color << 8) | (u8)c;
}

void vga_putchar(char c) {
    u16 vc = pack_vga_char(c, current_color);
    if (c == '\n') {
        usize row = vga_idx / 80;
        vga_idx = (row + 1) * 80;
    } else if (c == '\t') {
        vga_idx += 4;
    } else if (c == '\r') {
        usize row = vga_idx / 80;
        vga_idx = row * 80;
    } else {
        vga[vga_idx++] = vc;
    }

    if (vga_idx >= 80 * 25) {
        vga_clear();
    }
}

void vga_printstr(const char* str) {
    while (*str != '\0') {
        vga_putchar(*str++);
    }
}

void vga_setcolor(vga_color_t color) {
    current_color = color;
}

void vga_clearcolor() {
    current_color = default_color;
}

void vga_setdflcolor(vga_color_t color) {
    default_color = color;
}

void vga_clear() {
    u16 cv = pack_vga_char(' ', VGA_LIGHT_GREY);
    for (s32 i = 0; i < 80 * 25; i++) {
        vga[i] = cv;
    }
    vga_idx = 0;
}

static void vga_printf_write(const char* str, usize len) {
    for (usize i = 0; i < len; i++) {
        vga_putchar(str[i]);
    }
}

void vga_flush() {
    u16 pos = vga_idx;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (u8)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (u8)((pos >> 8) & 0xFF));
}

void printf(const char* fmt, ...) {
    va_list lst;
    va_start(lst, fmt);

    vwprintf(vga_printf_write, fmt, lst);

    va_end(lst);
}

int termctl(int code, int arg0) {
    switch (code) {
        case TCTL_FLUSH:
            vga_flush();
            return 0;
        case TCTL_CLEAR:
            vga_clear();
            return 0;
        case TCTL_SCLR:
            if (arg0 < 0 || arg0 > 15) return -1;
            vga_setcolor(arg0);
            return 0;
        case TCTL_CCLR:
            vga_setcolor(default_color);
            return 0;
        case TCTL_SDFL:
            if (arg0 < 0 || arg0 > 15) return -1;
            vga_setdflcolor(arg0);
            return 0;
        case TCTL_SIDX:
            vga_idx = arg0;
            return 0;
        default: return -1;
    }
}