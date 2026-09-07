#include "ssc.h"
#include "../ensurance.h"
#include <drivers/display/term.h>
#include <drivers/display/serial.h>
#include <drivers/display/fb.h>
#include <drivers/hid/kbd.h>
#include <drivers/hid/mouse.h>
#include <core/fd.h>
#include <core/kprint.h>

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
    if (!ensure_pointer((void*)args->a1, sizeof(framebuf_info_t), 1)) return -EINVAL;
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
    if (!ensure_pointer((void*)args->a1, args->a2, 1)) return -EINVAL;
    return create_fb_withmem(args->a0, (void*)args->a1, args->a2, (int*)args->a3);
}

DEFSYSCALL(sys_getmouseinfo) {
    if (!ensure_pointer((void*)args->a0, sizeof(mouse_info_t), 1)) return -EINVAL;
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

DEFSYSCALL(sys_setcurs) {
    if (!ensure_pointer((void*)args->a0, sizeof(term_pos_t), 0)) return -EINVAL;
    term_set_pos((term_pos_t*)args->a0, args->a1);
    return 0;
}

DEFSYSCALL(sys_getcurs) {
    if (!ensure_pointer((void*)args->a0, sizeof(term_pos_t), 1)) return -EINVAL;
    term_get_pos((term_pos_t*)args->a0);
    return 0;
}

#define FB_IOCTL_GETINFO 0x4600
#define FB_IOCTL_CLEAR   0x4601
#define FB_IOCTL_FLUSH   0x4602

DEFSYSCALL(sys_ioctl) {
    int fd = (int)args->a0;
    int cmd = (int)args->a1;
    void* data = (void*)args->a2;

    struct fdinfo* info = NULL;
    int ret = getfd(fd, &info);
    if (ret < 0) {
        return ret;
    }

    if (info->type == FDTYPE_IO) {
        switch (cmd) {
            case TCTL_FLUSH:
            case TCTL_CLEAR:
            case TCTL_SCLR:
            case TCTL_CCLR:
            case TCTL_AFLSH:
            case TCTL_GAFLH:
            case TCTL_NOECHO:
                return termctl(cmd, (int)(intptr_t)data);
            case TCTL_SETCURS:
                if (!ensure_pointer(data, sizeof(term_pos_t), 0)) return -EINVAL;
                term_set_pos((term_pos_t*)data, 0);
                return 0;
            case TCTL_GETCURS:
                if (!ensure_pointer(data, sizeof(term_pos_t), 1)) return -EINVAL;
                term_get_pos((term_pos_t*)data);
                return 0;
            default:
                return -ENOTTY;
        }
    } else if (info->type == FDTYPE_FB || info->type == FDTYPE_FBW) {
        switch (cmd) {
            case FB_IOCTL_GETINFO:
                if (!ensure_pointer(data, sizeof(framebuf_info_t), 1)) return -EINVAL;
                return get_fbinfo(fd, (framebuf_info_t*)data);
            case FB_IOCTL_CLEAR:
                clear_fb(fd);
                return 0;
            case FB_IOCTL_FLUSH:
                flush_scr();
                return 0;
            default:
                return -ENOTTY;
        }
    }

    return -ENOTTY;
}