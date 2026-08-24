#include <sys/types.h>
#pragma once
#define FBTYPE_TERM 0
#define FBTYPE_GUI  1

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

int create_fb(int type);
int switch_fb(int fb);
void clear_fb(int fb);
void flush_scr();
int get_fbinfo(int fb, framebuf_info_t* info);
int get_currfb();