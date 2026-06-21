#include <drivers/term.h>
#include <flanterm/flanterm.h>
#include <flanterm/flanterm_backends/fb.h>
#include <core/limreqs.h>
#include <lib/printf.h>

struct flanterm_context* _term_ctx;

void init_term() {
    struct limine_framebuffer* fb = fb_req.response->framebuffers[0];
    _term_ctx = flanterm_fb_init(
        NULL, NULL,
        fb->address,
        fb->width, fb->height,
        fb->pitch,
        fb->red_mask_size, fb->red_mask_shift,
        fb->green_mask_size, fb->green_mask_shift,
        fb->blue_mask_size, fb->blue_mask_shift,
        NULL, NULL, NULL,
        NULL, NULL, NULL, NULL,
        NULL, 
        0, 0, 0, 0, 0, 0,
        FLANTERM_FB_ROTATE_0
    );
    flanterm_set_autoflush(_term_ctx, true);
}

void term_putchar(char c) {
    if (c == '\n') {
        flanterm_write(_term_ctx, "\r\n", 2);
    } else {
        flanterm_write(_term_ctx, &c, 1);
    }
}

void term_puts(const char* str) {
    while (*str != '\0') {
        flanterm_write(_term_ctx, str++, 1);
    }
}

void term_setfgcolor(term_color_t clr) {
    bool bright = false;
    if (clr > TERM_GREY) {
        bright = true;
        clr -= TERM_GREY;
    }
    flanterm_set_text_fg(_term_ctx, clr, bright);
}

void term_setbgcolor(term_color_t clr) {
    bool bright = false;
    if (clr > TERM_GREY) {
        bright = true;
        clr -= TERM_GREY;
    }
    flanterm_set_text_bg(_term_ctx, clr, bright);
}

void term_rstfgcolor() {
    flanterm_reset_text_fg(_term_ctx);
}

void term_rstbgcolor() {
    flanterm_reset_text_bg(_term_ctx);
}

void term_clear() {
    flanterm_clear(_term_ctx, true);
}

void term_flush() {
    flanterm_flush(_term_ctx);
}

static void term_printf_write(const char* str, usize len) {
    for (usize i = 0; i < len; i++) {
        term_putchar(str[i]);
    }
}

void vprintf(const char* fmt, va_list lst) {
    vwprintf(term_printf_write, fmt, lst);
}

void printf(const char* fmt, ...) {
    va_list lst;
    va_start(lst, fmt);
    vprintf(fmt, lst);
    va_end(lst);
}

int termctl(int code, int arg0) {
    switch (code) {
        case TCTL_FLUSH:
            term_flush();
            return 0;
        case TCTL_CLEAR:
            term_clear();
            return 0;
        case TCTL_SCLR:
            if (arg0 < 0 || arg0 > 15) return -1;
            term_setfgcolor(arg0);
            return 0;
        case TCTL_CCLR:
            term_rstfgcolor();
            return 0;
        default: return -1;
    }
}