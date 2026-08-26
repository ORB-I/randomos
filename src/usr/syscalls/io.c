#include "ssc.h"
#include <drivers/display/term.h>
#include <drivers/display/serial.h>
#include <drivers/display/fb.h>
#include <drivers/hid/kbd.h>
#include <drivers/hid/mouse.h>

DEFSYSCALL(sys_termctl) {
    return termctl(args->a0, args->a1);
}

DEFSYSCALL(sys_createfb) {
    return create_fb(args->a0);
}

DEFSYSCALL(sys_switchfb) {
    return switch_fb(args->a0);
}

DEFSYSCALL(sys_clearfb) {
    clear_fb(args->a0);
    return 0;
}

DEFSYSCALL(sys_flushscr) {
    (void)args;
    flush_scr();
    return 0;
}

DEFSYSCALL(sys_getfbinf) {
    if (!args->a1) return -1;
    return get_fbinfo(args->a0, (framebuf_info_t*)args->a1);
}

DEFSYSCALL(sys_getcurfb) {
    (void)args;
    return get_currfb();
}

DEFSYSCALL(sys_getrawsc) {
    (void)args;
    return kbd_get_raw();
}

DEFSYSCALL(sys_createfbwmem) {
    return create_fb_withmem(args->a0, (void*)args->a1, args->a2, (int*)args->a3);
}

DEFSYSCALL(sys_getmouseinfo) {
    if (!args->a0) return -1;
    return get_mouse_info((mouse_info_t*)args->a0);
}

DEFSYSCALL(sys_serialwrite) {
    for (usize i = 0; i < args->a1; i++) {
        serial_putchar(((char*)args->a0)[i]);
    }
    return 0;
}

DEFSYSCALL(sys_getrawscto) {
    return kbd_getrawto(args->a0);
}