#include "sys/sysfn.h"
#include <fb.h>
#include <io.h>

int main() {
    int cfb = get_currfb();
    if (cfb < 0) {
        fprintf(STDERR, "failed to get current framebuffer\n");
        return 1;
    }

    framebuf_info_t info;
    if (get_fbinfo(cfb, &info) < 0) {
        fprintf(STDERR, "failed to get info on current framebuffer\n");
        return 1;
    }

    printf(
        "Base Address: %p\nFramebuffer Size: %lx\n",
        info.ptr, info.ptrsz
    );

    printf("Width: %lu Height: %lu Pitch: %lu BPP: %u\n", info.width, info.height, info.pitch, info.bpp);
    printf("Red Mask Size: %x Red Mask Shift: %x\n", info.mask_sizes[MASK_RED], info.mask_shifts[MASK_RED]);
    printf("Green Mask Size: %x Green Mask Shift: %x\n", info.mask_sizes[MASK_GREEN], info.mask_shifts[MASK_GREEN]);
    printf("Blue Mask Size: %x Blue Mask Shift: %x\n", info.mask_sizes[MASK_BLUE], info.mask_shifts[MASK_BLUE]);

    return 0;
}