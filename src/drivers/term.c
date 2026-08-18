#include <drivers/term.h>
#include <flanterm/flanterm.h>
#include <flanterm/flanterm_backends/fb.h>
#include <core/limreqs.h>
#include <lib/printf.h>
#include <drivers/fb.h>

struct flanterm_context* _term_ctx;
int _term_flush = 1;
int _term_cfb = -1;
int init_term(int fb) {
    framebuf_info_t fbinfo;
    if (get_fbinfo(fb, &fbinfo) < 0) return -1;

    _term_cfb = fb;
    _term_ctx = flanterm_fb_init(
        NULL, NULL,
        fbinfo.ptr,
        fbinfo.width, fbinfo.height,
        fbinfo.pitch,
        fbinfo.mask_sizes[MASK_RED], fbinfo.mask_shifts[MASK_RED],
        fbinfo.mask_sizes[MASK_GREEN], fbinfo.mask_shifts[MASK_GREEN],
        fbinfo.mask_sizes[MASK_BLUE], fbinfo.mask_shifts[MASK_BLUE],
        NULL, NULL, NULL,
        NULL, NULL, NULL, NULL,
        NULL, 
        0, 0, 0, 0, 0, 0,
        FLANTERM_FB_ROTATE_0
    );

    flanterm_set_autoflush(_term_ctx, false);
    return 0;
}

int get_termfb() {
    return _term_cfb;
}

void _term_flushscr() {
    flanterm_flush(_term_ctx);
    flush_scr();
}

// calling `term_putchar` is the absolute slowest way of writing stuff
// due to it having to copy the framebuffer over EVERY TIME
void term_putchar(char c) {
    if (c == '\n') {
        flanterm_write(_term_ctx, "\r\n", 2);
        if (_term_flush) _term_flushscr();
    } else {
        flanterm_write(_term_ctx, &c, 1);
        if (_term_flush) _term_flushscr();
    }
}

void term_write(const char* buf, usize sz) {
    int _term_flush_szd = _term_flush;
    _term_flush = 0;
    for (usize i = 0; i < sz; i++) {
        term_putchar(buf[i]);
    }
    _term_flush = _term_flush_szd;
    if (_term_flush) _term_flushscr();
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
    if (_term_flush) _term_flushscr();
}

void term_flush() {
    _term_flushscr();
}

void term_get_pos(term_pos_t* pos) {
    flanterm_get_cursor_pos(_term_ctx, &pos->x, &pos->y);
}

void term_set_pos(term_pos_t* pos, int flags) {
    if ((flags & TERMSET_ONLYX && flags & TERMSET_ONLYY) || flags == 0) {
        flanterm_set_cursor_pos(_term_ctx, pos->x, pos->y);
    } else {
        term_pos_t cpos;
        term_get_pos(&cpos);

        if (flags & TERMSET_ONLYX) flanterm_set_cursor_pos(_term_ctx, pos->x, cpos.y);
        if (flags & TERMSET_ONLYY) flanterm_set_cursor_pos(_term_ctx, cpos.x, pos->y);
    }
}

static void term_printf_write(const char* str, usize len) {
    term_write(str, len);
}

void vprintf(const char* fmt, va_list lst) {
    _term_flush = 0;
    vwprintf(term_printf_write, fmt, lst);
    _term_flush = 1;
    term_flush();
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
        case TCTL_AFLSH:
            _term_flush = arg0;
            return 0;
        case TCTL_GAFLH:
            return _term_flush;
        default: return -1;
    }
}