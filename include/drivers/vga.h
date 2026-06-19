#pragma once

#define TCTL_FLUSH 0
#define TCTL_CLEAR 1
#define TCTL_SCLR  2
#define TCTL_CCLR  3
#define TCTL_SDFL  4
#define TCTL_SIDX  5

typedef enum VgaColours {
    VGA_BLACK = 0,
    VGA_BLUE = 1,
    VGA_GREEN = 2,
    VGA_CYAN = 3,
    VGA_RED = 4,
    VGA_MAGENTA = 5,
    VGA_BROWN = 6,
    VGA_LIGHT_GREY = 7,
    VGA_DARK_GREY = 8,
    VGA_LIGHT_BLUE = 9,
    VGA_LIGHT_GREEN = 10,
    VGA_LIGHT_CYAN = 11,
    VGA_LIGHT_RED = 12,
    VGA_LIGHT_MAGENTA = 13,
    VGA_LIGHT_BROWN = 14,
    VGA_WHITE = 15
} vga_color_t;

void vga_putchar(char c);
void vga_printstr(const char* str);
void vga_setcolor(vga_color_t color);
void vga_setdflcolor(vga_color_t color);
void vga_clearcolor();
int termctl(int code, int arg0);
void vga_clear();
void vga_flush();
void vga_init();

void __attribute__((__format__(printf, 1, 2))) printf(const char* fmt, ...);