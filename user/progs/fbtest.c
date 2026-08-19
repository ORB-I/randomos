#include <fbdraw.h>
#include <fb.h>
#include <sys/sysfn.h>
#include <sys/syscall.h>
#include <mem.h>
#include <io.h>
#include <str.h>

static int termfb = -1;

static int fail(const char* msg) {
    if (termfb >= 0) switch_fb(termfb);
    fprintf(STDERR, "fbtest: %s\n", msg);
    return 1;
}

int main(void) {
    termfb = get_typefb(FBTYPE_TERM);
    if (termfb < 0) return fail("no term fb");

    int gui = create_fb(FBTYPE_GUI);
    if (gui < 0) return fail("create_fb gui");
    if (switch_fb(gui) < 0) return fail("switch_fb gui");

    int extra = create_fb(FBTYPE_GUI);
    if (extra < 0) return fail("create_fb extra");

    framebuf_info_t info;
    if (get_fbinfo(gui, &info) < 0) return fail("get_fbinfo gui");

    u32* fb = (u32*)info.ptr;
    u64 w = info.width;
    u64 h = info.height;
    u64 pitch = info.pitch / 4;

    if (w == 0 || h == 0 || pitch == 0) return fail("zero dims");

    guictx_t* g = gui_init(gui);
    if (!g) return fail("gui_init");

    u32 red = gui_rgb(g, 255, 0, 0);
    u32 grn = gui_rgb(g, 0, 255, 0);
    u32 blu = gui_rgb(g, 0, 0, 255);
    u32 wht = gui_rgb(g, 255, 255, 255);
    u32 blk = gui_rgb(g, 0, 0, 0);
    u32 yel = gui_rgb(g, 255, 255, 0);
    u32 mag = gui_rgb(g, 255, 0, 255);

    gui_fill_buf(fb, 0, 0, w, h, blk);

    gui_rectfill_buf(fb, 0, 0, w / 3, h, red);
    gui_rectfill_buf(fb, w / 3, 0, w / 3, h, grn);
    gui_rectfill_buf(fb, 2 * w / 3, 0, w / 3, h, blu);

    gui_rect_buf(fb, 10, 10, w - 20, h - 20, wht);
    gui_rectfill_buf(fb, 12, 12, w - 24, 4, wht);
    gui_rectfill_buf(fb, 12, h - 16, w - 24, 4, wht);

    for (u64 i = 0; i < w; i++) {
        fb[i] = wht;
        fb[(h - 1) * pitch + i] = wht;
    }
    for (u64 j = 0; j < h; j++) {
        fb[j * pitch + 0] = wht;
        fb[j * pitch + (w - 1)] = wht;
    }

    gui_str_buf(fb, 20, 30, "RandomOS fb test", wht, blk);
    gui_str_buf(fb, 20, 50, "RGB test OK", grn, blk);

    flush_scr();

    if (get_currfb() != gui) return fail("currfb wrong");

    if (switch_fb(extra) < 0) return fail("switch extra");
    if (get_currfb() != extra) return fail("currfb extra");

    clear_fb(extra);
    flush_scr();

    if (switch_fb(gui) < 0) return fail("switch back");
    if (get_currfb() != gui) return fail("currfb back");

    gui_fill_buf(fb, 0, 0, w, h, blk);
    gui_rectfill_buf(fb, 0, 0, w, h, yel);
    gui_rect_buf(fb, 0, 0, w, h, mag);
    gui_str_buf(fb, 40, 80, "PASS", wht, mag);
    flush_scr();

    gui_free(g);
    switch_fb(termfb);
    printf("fbtest: OK %ux%u pitch=%u bpp=%u\n",
           (unsigned)w, (unsigned)h, (unsigned)pitch, (unsigned)info.bpp);
    return 0;
}