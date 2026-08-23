#pragma once
#include <core/limine.h>
#include <core/std.h>

#define FBTYPE_TERM 0
#define FBTYPE_GUI  1
typedef struct {
    u64 width, height, pitch;
    usize ptrsz;
    int type;
    void* ptr;
} framebuf_t;

#define MASK_RED   0
#define MASK_GREEN 1
#define MASK_BLUE  2
typedef struct {
    void* ptr; usize ptrsz;
    u64 width, height, pitch;
    u16 bpp;
    u8 mask_sizes[3];
    u8 mask_shifts[3];
} framebuf_info_t;

#define MAX_FBS 10
typedef struct FrameBufferDriverCtx {
    struct limine_framebuffer* backend;
    framebuf_t* fbs[MAX_FBS];
    usize nfbs;
    int cfb;
} fbdrv_ctx_t;

int init_fbdrv(struct limine_framebuffer* lmfb);
void deinit_fbdrv();

int create_fb(int type);
void free_fb(int fb);
int switch_fb(int fb);
int get_fbinfo(int fb, framebuf_info_t* info);

usize create_fb_withmem(int type, void* ptr, usize sz, int* fbdes);
void free_fb_withmem(int fb);

void clear_fb(int fb);
void flush_scr();

int get_typefb(int type);
int get_currfb();