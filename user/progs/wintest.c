#include <fbdraw.h>
#include <fb.h>
#include <io.h>
#include <sys/sysfn.h>
#include <kbd.h>
#include <mouse.h>

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
    u32 red = gui_rgb(gctx, 255, 0, 0);
    gui_fill_buf((u32*)info.ptr, 0, 0, info.width, info.height, blue);

    switch_fb(gui);
    flush_scr();

    int x = info.width/2, y = info.height/2;
    mouse_info_t minfo = {0, 0, 0};
    while (1) {
        if (get_mouse_info(&minfo) < 0) {
            serial_printf("mouse failed\r\n");
        } else {
            x += minfo.x;
            y += minfo.y;
            serial_printf("x=%d,y=%d\r\n", x, y);
        }

        gui_fill_buf((u32*)info.ptr, 0, 0, info.width, info.height, blue);
        gui_rectfill(gctx, x, y, 10, 10, red);
        flush_scr();
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