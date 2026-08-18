#pragma once

#include <sys/types.h>
#include <fb.h>

#define GUI_FONT_W 8
#define GUI_FONT_H 16
#define GUI_FONT_FIRST 0x20

extern const u8 gui_font[256][GUI_FONT_H];
extern u64 gui_pitch;

typedef struct {
    u32* fb;
    u64 w, h, pitch;
    u32 bpp;
    u8 rs, gs, bs;
} guictx_t;

guictx_t* gui_init(int fb);
void gui_free(guictx_t* g);

void gui_pixel(guictx_t* g, u64 x, u64 y, u32 c);
void gui_hline(guictx_t* g, u64 x, u64 y, u64 len, u32 c);
void gui_vline(guictx_t* g, u64 x, u64 y, u64 len, u32 c);
void gui_rect(guictx_t* g, u64 x, u64 y, u64 w, u64 h, u32 c);
void gui_rectfill(guictx_t* g, u64 x, u64 y, u64 w, u64 h, u32 c);
void gui_fill(guictx_t* g, u32 c);

void gui_rectfill_buf(u32* fb, u64 x, u64 y, u64 w, u64 h, u32 c);
void gui_rect_buf(u32* fb, u64 x, u64 y, u64 w, u64 h, u32 c);
void gui_str_buf(u32* fb, u64 x, u64 y, const char* s, u32 fg, u32 bg);
void gui_char_buf(u32* fb, u64 x, u64 y, char c, u32 fg, u32 bg);
void gui_fill_buf(u32* fb, u64 x, u64 y, u64 w, u64 h, u32 c);

void gui_char(guictx_t* g, u64 x, u64 y, char c, u32 fg, u32 bg);
void gui_str(guictx_t* g, u64 x, u64 y, const char* s, u32 fg, u32 bg);
void gui_str_len(guictx_t* g, u64 x, u64 y, const char* s, usize n, u32 fg, u32 bg);

void gui_blit(guictx_t* g, u64 x, u64 y, u32* src, u64 w, u64 h, u64 spitch);

u32 gui_rgb(guictx_t* g, u8 r, u8 green, u8 b);