#include <fb.h>
#include <sys/syscall.h>

int create_fb(int type) {
    return __syscall1(SYS_CREATEFB, type);
}

void rmfb(int fb) {
    __syscall1(SYS_RMFB, fb);
}

int switch_fb(int fb) {
    return __syscall1(SYS_SWITCHFB, fb);
}

void clear_fb(int fb) {
    __syscall1(SYS_CLEARFB, fb);
}

void flush_scr() {
    __syscall0(SYS_FLUSHSCR);
}

int get_fbinfo(int fb, framebuf_info_t* info) {
    return __syscall2(SYS_GETFBINF, fb, (u64)info);
}

int get_typefb(int type) {
    return __syscall1(SYS_GETFBTYP, type);
}

int get_currfb() {
    return __syscall0(SYS_GETCURFB);
}