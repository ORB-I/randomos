#include <core/limine.h>
#include <drivers/display/fb.h>
#include <lib/string.h>
#include <core/fd.h>
#include <core/liballoc.h>

fbdrv_ctx_t* fbctx = NULL;

int init_fbdrv(struct limine_framebuffer* lmfb) {
    if (!lmfb) return -1;
    if (fbctx) return 0;
    fbctx = malloc(sizeof(*fbctx));
    if (!fbctx) return -1;

    fbctx->backend = lmfb;
    fbctx->cfb = -1;

    return 0;
}

void deinit_fbdrv() {
    if (!fbctx) return;
    free(fbctx);
}

int create_fb(int type) {
    if (!fbctx) return -1;
    struct fdinfo info = {
        .fd = -1,
        .inuse = 0,
        .type = FDTYPE_FB
    };
    struct fdinfo* fd = getnewfd(&info);
    if (!fd) {
        return -1;
    }

    fd->data.fb = malloc(sizeof(framebuf_t));
    if (!fd->data.fb) {
        closefd(fd->fd);
        return -1;
    }

    fd->data.fb->height = fbctx->backend->height;
    fd->data.fb->width  = fbctx->backend->width;
    fd->data.fb->pitch  = fbctx->backend->pitch;
    fd->data.fb->type   = type;
    fd->data.fb->ptrsz = fd->data.fb->pitch * fd->data.fb->height;
    fd->data.fb->ptr    = malloc(fd->data.fb->ptrsz);

    if (!fd->data.fb->ptr) {
        free(fd->data.fb);
        closefd(fd->fd);
        return -1;
    }

    return fd->fd;
}

usize create_fb_withmem(int type, void* ptr, usize sz, int* fbdes) {
    if (!fbctx) return -1;
    struct fdinfo info = {
        .fd = -1,
        .inuse = 0,
        .type = FDTYPE_FBW
    };
    struct fdinfo* fd = getnewfd(&info);
    if (!fd) {
        return -1;
    }

    fd->data.fb = malloc(sizeof(framebuf_t));
    if (!fd->data.fb) {
        closefd(fd->fd);
        return -1;
    }

    fd->data.fb->height = fbctx->backend->height;
    fd->data.fb->width  = fbctx->backend->width;
    fd->data.fb->pitch  = fbctx->backend->pitch;
    fd->data.fb->type   = type;

    if (!ptr || sz == 0) {
        usize nsz = fd->data.fb->pitch * fd->data.fb->height;
        free(fd->data.fb);
        closefd(fd->fd);
        return nsz;
    }

    fd->data.fb->ptrsz  = sz;
    fd->data.fb->ptr    = ptr;

    if (!fd->data.fb->ptr) {
        free(fd->data.fb);
        closefd(fd->fd);
        return -1;
    }

    *fbdes = fd->fd;
    return 0;
}

int free_fb_withmem(int fb) {
    struct fdinfo* info;
    if (getfd(fb, &info) < 0) {
        return -1;
    }
    
    if (!info->data.fb) return -1;
    free(info->data.fb);
    return closefd(fb);
}

int free_fb(int fb) {
    struct fdinfo* info;
    if (getfd(fb, &info) < 0) {
        return -1;
    }
    
    if (!info->data.fb) return -1;
    free(info->data.fb->ptr);
    free(info->data.fb);
    return closefd(fb);
}

int switch_fb(int fb) {
    struct fdinfo* info;
    if (getfd(fb, &info) < 0) {
        return -1;
    }

    if (!fbctx) return -1;
    if (!info->data.fb) return -1;
    fbctx->cfb = fb;
    return 0;
}

void clear_fb(int fb) {
    struct fdinfo* info;
    if (getfd(fb, &info) < 0) {
        return;
    }

    memset(info->data.fb->ptr, 0, info->data.fb->ptrsz);
}

void flush_scr() {
    struct fdinfo* info;
    if (getfd(fbctx->cfb, &info) < 0) {
        return;
    }
    memcpy(fbctx->backend->address, info->data.fb->ptr, info->data.fb->ptrsz);
}

int get_fbinfo(int fb, framebuf_info_t *info) {
    if (!info || !fbctx) return -1;
    struct fdinfo* finfo;
    if (getfd(fb, &finfo) < 0) {
        return -1;
    }
    
    framebuf_t* fbp = finfo->data.fb;
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

int get_currfb() {
    return fbctx->cfb;
}