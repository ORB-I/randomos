#include <fb.h>
#include <mem.h>
#include <sys/syscall.h>

int create_fb(int type) {
    int fb;
    usize sz = __syscall4(SYS_CREATEFBWMEM, type, 0, 0, (u64)&fb);
    if (sz < 0) return -1;
    void* mem = malloc(sz);
    if (!mem) return -1;
    if (__syscall4(SYS_CREATEFBWMEM, type, (u64)mem, sz, (u64)&fb) < 0) {
        free(mem);
        return -1;
    }
    return fb;
}

void rmfb(int fb) {
    framebuf_info_t info;
    get_fbinfo(fb, &info);
    free(info.ptr);
    __syscall1(SYS_RMFBWMEM, fb);
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