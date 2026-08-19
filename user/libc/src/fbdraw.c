#include <fbdraw.h>
#include <fb.h>
#include <mem.h>
#include <str.h>

#include <gui_font.h>

u64 gui_pitch = 0;

guictx_t* gui_init(int fb) {
    framebuf_info_t info;
    if (get_fbinfo(fb, &info) < 0) return NULL;

    guictx_t* g = (guictx_t*)malloc(sizeof(guictx_t));
    if (!g) return NULL;

    g->fb = (u32*)info.ptr;
    g->w = info.width;
    g->h = info.height;
    g->pitch = info.pitch / 4;
    g->bpp = info.bpp;
    g->rs = info.mask_shifts[0];
    g->gs = info.mask_shifts[1];
    g->bs = info.mask_shifts[2];
    gui_pitch = g->pitch;
    return g;
}

void gui_free(guictx_t* g) {
    if (g) free(g);
}

u32 gui_rgb(guictx_t* g, u8 r, u8 green, u8 b) {
    return ((u32)r << g->rs) | ((u32)green << g->gs) | ((u32)b << g->bs);
}

void gui_pixel(guictx_t* g, u64 x, u64 y, u32 c) {
    if (x >= g->w || y >= g->h) return;
    g->fb[y * g->pitch + x] = c;
}

void gui_hline(guictx_t* g, u64 x, u64 y, u64 len, u32 c) {
    for (u64 i = 0; i < len; i++) gui_pixel(g, x + i, y, c);
}

void gui_vline(guictx_t* g, u64 x, u64 y, u64 len, u32 c) {
    for (u64 i = 0; i < len; i++) gui_pixel(g, x, y + i, c);
}

void gui_rect(guictx_t* g, u64 x, u64 y, u64 w, u64 h, u32 c) {
    gui_hline(g, x, y, w, c);
    gui_hline(g, x, y + h - 1, w, c);
    gui_vline(g, x, y, h, c);
    gui_vline(g, x + w - 1, y, h, c);
}

void gui_rectfill(guictx_t* g, u64 x, u64 y, u64 w, u64 h, u32 c) {
    for (u64 j = 0; j < h; j++) gui_hline(g, x, y + j, w, c);
}

void gui_fill(guictx_t* g, u32 c) {
    gui_rectfill(g, 0, 0, g->w, g->h, c);
}

void gui_rectfill_buf(u32* fb, u64 x, u64 y, u64 w, u64 h, u32 c) {
    for (u64 j = 0; j < h; j++) {
        for (u64 i = 0; i < w; i++) {
            fb[(y + j) * gui_pitch + (x + i)] = c;
        }
    }
}

void gui_rect_buf(u32* fb, u64 x, u64 y, u64 w, u64 h, u32 c) {
    for (u64 i = 0; i < w; i++) {
        fb[y * gui_pitch + (x + i)] = c;
        fb[(y + h - 1) * gui_pitch + (x + i)] = c;
    }
    for (u64 j = 0; j < h; j++) {
        fb[(y + j) * gui_pitch + x] = c;
        fb[(y + j) * gui_pitch + (x + w - 1)] = c;
    }
}

void gui_char_buf(u32* fb, u64 x, u64 y, char c, u32 fg, u32 bg) {
    if (c < GUI_FONT_FIRST) return;
    const u8* rows = gui_font[(u8)c];
    for (u8 gy = 0; gy < GUI_FONT_H; gy++) {
        u8 byte = rows[gy];
        for (u8 gx = 0; gx < GUI_FONT_W; gx++) {
            fb[(y + gy) * gui_pitch + (x + gx)] = (byte & (1 << (7 - gx))) ? fg : bg;
        }
    }
}

void gui_str_buf(u32* fb, u64 x, u64 y, const char* s, u32 fg, u32 bg) {
    for (usize i = 0; s[i]; i++) {
        gui_char_buf(fb, x + i * GUI_FONT_W, y, s[i], fg, bg);
    }
}

void gui_fill_buf(u32* fb, u64 x, u64 y, u64 w, u64 h, u32 c) {
    for (u64 j = 0; j < h; j++) {
        for (u64 i = 0; i < w; i++) {
            fb[(y + j) * gui_pitch + (x + i)] = c;
        }
    }
}

void gui_char(guictx_t* g, u64 x, u64 y, char c, u32 fg, u32 bg) {
    if (c < GUI_FONT_FIRST) return;
    const u8* rows = gui_font[(u8)c];
    for (u8 gy = 0; gy < GUI_FONT_H; gy++) {
        u8 byte = rows[gy];
        for (u8 gx = 0; gx < GUI_FONT_W; gx++) {
            if (byte & (1 << (7 - gx))) {
                gui_pixel(g, x + gx, y + gy, fg);
            } else {
                gui_pixel(g, x + gx, y + gy, bg);
            }
        }
    }
}

void gui_str_len(guictx_t* g, u64 x, u64 y, const char* s, usize n, u32 fg, u32 bg) {
    for (usize i = 0; i < n && s[i]; i++) {
        gui_char(g, x + i * GUI_FONT_W, y, s[i], fg, bg);
    }
}

void gui_str(guictx_t* g, u64 x, u64 y, const char* s, u32 fg, u32 bg) {
    gui_str_len(g, x, y, s, strlen(s), fg, bg);
}

void gui_blit(guictx_t* g, u64 x, u64 y, u32* src, u64 w, u64 h, u64 spitch) {
    for (u64 j = 0; j < h; j++) {
        for (u64 i = 0; i < w; i++) {
            gui_pixel(g, x + i, y + j, src[j * spitch + i]);
        }
    }
}