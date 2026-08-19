#include <fbdraw.h>
#include <fb.h>
#include <io.h>
#include <sys/sysfn.h>
#include <kbd.h>

static int initfb = -1;

static int fail(const char* msg) {
    if (initfb >= 0) switch_fb(initfb);
    fprintf(STDERR, "wintest: %s\n", msg);
    return 1;
}

int main() {
    initfb = get_currfb();
    if (initfb < 0) return fail("Failed to get current framebuffer");

    int gui = create_fb(FBTYPE_GUI);
    if (gui < 0) return fail("Failed to create GUI framebuffer");

    framebuf_info_t info;
    if (get_fbinfo(gui, &info) < 0) return fail("Failed to get framebuffer info");

    if (info.width == 0 || info.height == 0 || info.pitch == 0) return fail("Framebuffer 0 size");

    guictx_t* gctx = gui_init(gui);
    if (!gctx) return fail("Failed to initialize GUI drawing");

    u32 blue = gui_rgb(gctx, 0, 0, 255);
    gui_fill_buf((u32*)info.ptr, 0, 0, info.width, info.height, blue);

    switch_fb(gui);
    flush_scr();

    while (1) {
        int sc = kbd_get_raw();
        if (sc == 0x01) {
            break;
        }
    }

    gui_free(gctx);
    rmfb(gui);
    switch_fb(initfb);

    return 0;
}