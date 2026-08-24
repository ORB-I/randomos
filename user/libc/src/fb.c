#include <fb.h>
#include <mem.h>
#include <sys/syscall.h>

int create_fb(int type) {
    int fb;
    s64 sz = (s64)__syscall4(SYS_CREATEFBWMEM, type, 0, 0, (u64)&fb);
    if (sz <= 0) return -1;
    void* mem = malloc((usize)sz);
    if (!mem) return -1;
    if (__syscall4(SYS_CREATEFBWMEM, type, (u64)mem, sz, (u64)&fb) < 0) {
        free(mem);
        return -1;
    }
    return fb;
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

int get_currfb() {
    return __syscall0(SYS_GETCURFB);
}