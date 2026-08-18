#include <fbdraw.h>
#include <fb.h>
#include <kbd.h>
#include <mem.h>
#include <io.h>
#include <sys/sysfn.h>
#include <sys/syscall.h>
#include <string.h>

#define WIN_MAX 16
#define TITLE_H 24
#define WIN_W 320
#define WIN_H 200

typedef struct {
    bool open;
    u32* buf;
    u64 x, y;
    u64 w, h;
    const char* title;
} win_t;

static win_t wins[WIN_MAX];
static u32* gui_fb = NULL;
static u64 gui_w = 0, gui_h = 0;
static int gui_fb_id = -1;
static int term_fb = -1;

static u32 col_bg, col_bg2, col_title, col_border, col_text, col_white;
static u32 col_hl, col_hld;

static void wm_flush(void) {
    flush_scr();
}

static u32* wm_alloc(u64 w, u64 h) {
    u64 sz = w * h;
    u32* p = (u32*)malloc(sz * 4);
    if (p) memset(p, 0, sz * 4);
    return p;
}

static void wm_blit(u32* dst, u64 dx, u64 dy, u32* src, u64 sw, u64 sh) {
    for (u64 j = 0; j < sh; j++) {
        for (u64 i = 0; i < sw; i++) {
            dst[(dy + j) * gui_pitch + (dx + i)] = src[j * sw + i];
        }
    }
}

static void draw_win(u32* dst, win_t* w) {
    if (!w->buf) return;

    gui_rectfill_buf(dst, w->x, w->y, w->w, w->h, col_bg);
    gui_rect_buf(dst, w->x, w->y, w->w, w->h, col_border);
    gui_rectfill_buf(dst, w->x, w->y, w->w, TITLE_H, col_title);
    gui_rect_buf(dst, w->x, w->y, w->w, TITLE_H, col_border);

    gui_str_buf(dst, w->x + 8, w->y + 6, w->title, col_text, col_bg);
}

static void wm_compose(void) {
    gui_fill_buf(gui_fb, 0, 0, gui_w, gui_h, col_bg2);

    for (int i = 0; i < WIN_MAX; i++) {
        if (wins[i].open) draw_win(gui_fb, &wins[i]);
    }
}

static int wm_create(const char* title, u64 x, u64 y, u64 w, u64 h) {
    for (int i = 0; i < WIN_MAX; i++) {
        if (!wins[i].open) {
            wins[i].open = true;
            wins[i].x = x;
            wins[i].y = y;
            wins[i].w = w;
            wins[i].h = h;
            wins[i].title = title;
            wins[i].buf = wm_alloc(w, h);
            if (!wins[i].buf) {
                wins[i].open = false;
                return -1;
            }
            gui_rectfill_buf(wins[i].buf, 0, 0, w, h, col_bg);
            gui_rect_buf(wins[i].buf, 0, 0, w, h, col_border);
            gui_rectfill_buf(wins[i].buf, 0, 0, w, TITLE_H, col_title);
            gui_str_buf(wins[i].buf, 8, 6, title, col_text, col_bg);
            return i;
        }
    }
    return -1;
}

static void wm_destroy(int id) {
    if (id < 0 || id >= WIN_MAX || !wins[id].open) return;
    if (wins[id].buf) free(wins[id].buf);
    wins[id].open = false;
}

static int wm_top(void) {
    int top = -1;
    for (int i = 0; i < WIN_MAX; i++) {
        if (wins[i].open) top = i;
    }
    return top;
}

static bool wm_hit(u64 x, u64 y, int* out) {
    int top = wm_top();
    for (int i = top; i >= 0; i--) {
        if (!wins[i].open) continue;
        if (x >= wins[i].x && x < wins[i].x + wins[i].w &&
            y >= wins[i].y && y < wins[i].y + wins[i].h) {
            *out = i;
            return true;
        }
    }
    return false;
}

static void wm_focus(int id) {
    win_t tmp = wins[id];
    for (int i = id; i < WIN_MAX - 1; i++) {
        if (wins[i + 1].open) {
            wins[i] = wins[i + 1];
        } else {
            break;
        }
    }
    int last = wm_top();
    if (last >= 0 && last < WIN_MAX) {
        wins[last] = tmp;
    }
}

static void wm_redraw(int id) {
    if (id < 0 || id >= WIN_MAX || !wins[id].open) return;
    if (wins[id].buf) {
        wm_blit(gui_fb, wins[id].x, wins[id].y, wins[id].buf, wins[id].w, wins[id].h);
    }
}

int main(void) {
    term_fb = get_typefb(FBTYPE_TERM);
    if (term_fb < 0) return 1;

    gui_fb_id = create_fb(FBTYPE_GUI);
    if (gui_fb_id < 0) return 1;
    if (switch_fb(gui_fb_id) < 0) return 1;

    framebuf_info_t info;
    if (get_fbinfo(gui_fb_id, &info) < 0) return 1;

    gui_fb = (u32*)info.ptr;
    gui_w = info.width;
    gui_h = info.height;

    guictx_t* g = gui_init(gui_fb_id);
    if (!g) return 1;

    col_bg = gui_rgb(g, 30, 30, 38);
    col_bg2 = gui_rgb(g, 20, 20, 24);
    col_title = gui_rgb(g, 60, 60, 90);
    col_border = gui_rgb(g, 120, 120, 140);
    col_text = gui_rgb(g, 230, 230, 240);
    col_white = gui_rgb(g, 255, 255, 255);
    col_hl = gui_rgb(g, 90, 90, 120);
    col_hld = gui_rgb(g, 120, 120, 160);

    gui_free(g);

    memset(wins, 0, sizeof(wins));

    wm_create("Terminal", 60, 60, WIN_W, WIN_H);
    wm_create("Files", 120, 100, WIN_W, WIN_H);

    wm_compose();
    wm_flush();

    bool running = true;
    u32 next = 0;

    while (running) {
        u8 sc = kbd_get_raw();
        if (sc == 0x01) {
            running = false;
            continue;
        }

        if (sc == 0x3B) {
            u64 nx = 40 + (u64)(next % 5) * 40;
            u64 ny = 40 + (u64)((next / 5) % 4) * 40;
            next++;
            int id = wm_create("Window", nx, ny, WIN_W, WIN_H);
            if (id >= 0) wm_focus(id);
        }

        if (sc == 0x0C) {
            int id = wm_top();
            if (id >= 0) wm_destroy(id);
        }

        if (sc == 0x35) {
            termctl(TCTL_AFLSH, 0);
            termctl(TCTL_SCLR, 7);
            printf("wm: %ux%u\n", (unsigned)gui_w, (unsigned)gui_h);
            termctl(TCTL_CCLR, 0);
            termctl(TCTL_AFLSH, 1);
        }

        if (sc == 0x1C) {
            int id = wm_top();
            if (id >= 0) {
                gui_rectfill_buf(wins[id].buf, 8, 8, 160, 16, col_hl);
                gui_str_buf(wins[id].buf, 10, 11, "Hello from RandomOS", col_white, col_hl);
                wm_redraw(id);
                wm_flush();
            }
        }

        sleep(0);
    }

    switch_fb(term_fb);
    return 0;
}