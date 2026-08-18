#include "core/limine.h"
#include <drivers/fb.h>
#include <lib/string.h>
#include <core/liballoc.h>

fbdrv_ctx_t* fbctx = NULL;

int init_fbdrv(struct limine_framebuffer* lmfb) {
    if (!lmfb) return -1;
    if (fbctx) return 0;
    fbctx = malloc(sizeof(*fbctx));
    if (!fbctx) return -1;

    fbctx->backend = lmfb;
    fbctx->nfbs = 0;
    fbctx->cfb = -1;

    return 0;
}

void deinit_fbdrv() {
    if (!fbctx) return;
    for (usize i = 0; i < fbctx->nfbs; i++) {
        fbctx->fbs[i]->height = fbctx->fbs[i]->width = 0;
        free(fbctx->fbs[i]->ptr);
        free(fbctx->fbs[i]);
    }

    free(fbctx);
}

int create_fb(int type) {
    if (!fbctx) return -1;
    if (fbctx->nfbs == MAX_FBS) return -1;

    int fb = fbctx->nfbs++;
    fbctx->fbs[fb] = malloc(sizeof(framebuf_t));
    if (!fbctx->fbs[fb]) {
        fbctx->nfbs--;
        return -1;
    }

    fbctx->fbs[fb]->height = fbctx->backend->height;
    fbctx->fbs[fb]->width  = fbctx->backend->width;
    fbctx->fbs[fb]->pitch  = fbctx->backend->pitch;
    fbctx->fbs[fb]->type   = type;
    fbctx->fbs[fb]->ptrsz = fbctx->fbs[fb]->pitch * fbctx->fbs[fb]->height;
    fbctx->fbs[fb]->ptr    = malloc(fbctx->fbs[fb]->ptrsz);

    if (!fbctx->fbs[fb]->ptr) {
        free(fbctx->fbs[fb]);
        fbctx->nfbs--;
        return -1;
    }

    return fb;
}

void free_fb(int fb) {
    if (!fbctx) return;

    framebuf_t* fbp = fbctx->fbs[fb];
    if (!fbp) return;

    fbctx->nfbs--;
    free(fbp->ptr);
    free(fbp);
}

int switch_fb(int fb) {
    if (!fbctx) return -1;
    if (!fbctx->fbs[fb]) return -1;
    fbctx->cfb = fb;
    return 0;
}

void clear_fb(int fb) {
    memset(fbctx->fbs[fb]->ptr, 0, fbctx->fbs[fb]->ptrsz);
}

void flush_scr() {
    memcpy(fbctx->backend->address, fbctx->fbs[fbctx->cfb]->ptr, fbctx->fbs[fbctx->cfb]->ptrsz);
}

int get_fbinfo(int fb, framebuf_info_t *info) {
    if (!info || !fbctx) return -1;
    framebuf_t* fbp = fbctx->fbs[fb];
    if (!fbp) return -1;
    struct limine_framebuffer* bep = fbctx->backend;

    info->ptr   = fbp->ptr;
    info->ptrsz = fbp->ptrsz;

    info->width  = fbp->width;
    info->height = fbp->height;
    info->pitch  = fbp->pitch;
    info->bpp    = bep->bpp;

    info->mask_sizes[MASK_RED]   = bep->red_mask_size;
    info->mask_sizes[MASK_GREEN] = bep->green_mask_size;
    info->mask_sizes[MASK_BLUE]  = bep->blue_mask_size;

    info->mask_shifts[MASK_RED]   = bep->red_mask_shift;
    info->mask_shifts[MASK_GREEN] = bep->green_mask_shift;
    info->mask_shifts[MASK_BLUE]  = bep->blue_mask_shift;

    return 0;
}

int get_typefb(int type) {
    for (usize i = 0; i < fbctx->nfbs; i++) {
        if (fbctx->fbs[i]->type == type) {
            return i;
        }
    }
    return -1;
}

int get_currfb() {
    return fbctx->cfb;
}