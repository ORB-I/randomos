#pragma once
#include <stdarg.h>

#define TCTL_FLUSH 0
#define TCTL_CLEAR 1
#define TCTL_SCLR  2
#define TCTL_CCLR  3

typedef enum {
    TERM_BLACK = 0,
    TERM_RED = 1,
    TERM_GREEN = 2,
    TERM_BROWN = 3,
    TERM_BLUE = 4,
    TERM_MAGENTA = 5,
    TERM_CYAN = 6,
    TERM_GREY = 7,
    TERM_BRIGHT_BLACK = 8,
    TERM_BRIGHT_RED = 9,
    TERM_BRIGHT_GREEN = 10,
    TERM_BRIGHT_BROWN = 11,
    TERM_BRIGHT_BLUE = 12,
    TERM_BRIGHT_MAGENTA = 13,
    TERM_BRIGHT_CYAN = 14,
    TERM_BRIGHT_GREY = 15,
} term_color_t;

void init_term();
void term_putchar(char c);
void term_puts(const char* str);
void term_setfgcolor(term_color_t clr);
void term_setbgcolor(term_color_t clr);
void term_rstfgcolor();
void term_rstbgcolor();
void term_clear();
void term_flush();
void printf(const char* fmt, ...);
void vprintf(const char* fmt, va_list lst);
int termctl(int code, int arg0);