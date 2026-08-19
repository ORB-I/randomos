#include <core/std.h>
#include <core/asmh.h>
#include <core/idt.h>
#include <drivers/apic.h>
#include <lib/string.h>
#include <drivers/ps2/mouse.h>
#include <drivers/mouse.h>

#define MOUSEBUF_SZ 256
mouse_info_t mousebuf[MOUSEBUF_SZ];
u32 mb_head = 0;
u32 mb_tail = 0;
bool mb_full = 0;
int mb_type = 0;

int get_mouse_info(mouse_info_t* buf) {
    if (mb_type == 0) return -1;
    mouse_info_t info = dequeue_mouse();
    memcpy(buf, &info, sizeof(info));
    return 0;
}

void enqueue_mouse(mouse_info_t info) {
    if (!mb_full) {
        memcpy(&mousebuf[mb_head], &info, sizeof(info));

        mb_head = (mb_head + 1) % MOUSEBUF_SZ;
        if (mb_head == mb_tail) {
            mb_full = true;
        }
    }
}

mouse_info_t dequeue_mouse() {
    if (mb_head == mb_tail && !mb_full) {
        return (mouse_info_t){0,0,0};
    }

    mouse_info_t info = mousebuf[mb_tail];
    mb_tail = (mb_tail + 1) % MOUSEBUF_SZ;
    mb_full = false;
    return info;
}

int init_mouse(int type) {
    mb_type = type;
    if (MOUSETYPE_USBHID) {
        return -1;
    } else {
        init_mouseps2();
        return 0;
    }
}