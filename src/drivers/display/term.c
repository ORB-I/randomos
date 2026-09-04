#include <drivers/display/term.h>
#include <flanterm/flanterm.h>
#include <flanterm/flanterm_backends/fb.h>
#include <core/limreqs.h>
#include <lib/string.h>
#include <drivers/hid/kbd.h>
#include <drivers/display/fb.h>
#include <scheduler/process.h>
#include <core/errno.h>
#include <core/spinlock.h>

struct flanterm_context* _term_ctx;
int _term_flush = 1;
static spinlock_t _term_lock = SPINLOCK_INIT;

int init_term(int fb) {
    framebuf_info_t fbinfo;

    int ret = 0;
    if ((ret = get_fbinfo(fb, &fbinfo)) < 0) return ret;

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

void _term_flushscr() {
    flanterm_flush(_term_ctx);
    flush_scr();
}

// calling `term_putchar` is the absolute slowest way of writing stuff
// due to it having to copy the framebuffer over EVERY TIME
void term_putchar(char c) {
    spinlock_acquire(&_term_lock);
    if (c == '\n') {
        flanterm_write(_term_ctx, "\r\n", 2);
        if (_term_flush) _term_flushscr();
    } else {
        flanterm_write(_term_ctx, &c, 1);
        if (_term_flush) _term_flushscr();
    }
    spinlock_release(&_term_lock);
}

void term_write(const char* buf, usize sz) {
    spinlock_acquire(&_term_lock);
    usize start = 0;
    for (usize i = 0; i <= sz; i++) {
        if (i == sz || buf[i] == '\n') {
            if (i > start) {
                flanterm_write(_term_ctx, buf + start, i - start);
            }
            if (i < sz) {
                flanterm_write(_term_ctx, "\r\n", 2);
            }
            start = i + 1;
        }
    }
    if (_term_flush) _term_flushscr();
    spinlock_release(&_term_lock);
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

int termctl(int code, int arg0) {
    switch (code) {
        case TCTL_FLUSH:
            term_flush();
            return 0;
        case TCTL_CLEAR:
            term_clear();
            return 0;
        case TCTL_SCLR:
            if (arg0 < 0 || arg0 > 15) return -EINVAL;
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
        case TCTL_NOECHO:
            noecho(arg0);
        default: return -EINVAL;
    }
}