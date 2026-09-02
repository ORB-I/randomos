#include <core/std.h>
#include <core/errno.h>
#include <core/liballoc.h>
#include <core/udevr.h>
#include <core/fd.h>
#include <core/printf.h>
#include <lib/string.h>
#include <drivers/storage/fs.h>

typedef struct {
    u8 used;
    char name[32];
    u16 id;
    u16 flags;
    udevfn_t read;
    udevfn_t write;
} udrv_t;

static udrv_t* udevr = NULL;
static usize udevrav = 0;
static int udevr_devfs = 0;

int udevr_init() {
    udevr = malloc(sizeof(udrv_t) * 128);
    if (!udevr) return -ENOMEM;
    memset(udevr, 0, sizeof(udrv_t) * 128);
    udevrav = 128;

    if (mount(NULL, "/dev", "ramfs") < 0) {
        udevr_devfs = 0;
    } else {
        udevr_devfs = 1;
    }

    return 0;
}

int udevr_register(u16 id, const char name[32], u16 flags, udevfn_t read, udevfn_t write) {
    if (!udevr) return -ENOEXIST;
    for (usize i = 0; i < udevrav; i++) {
        if (!udevr[i].used) {
            udevr[i].used = 1;
            udevr[i].id = id;
            udevr[i].flags = flags;
            udevr[i].read = read;
            udevr[i].write = write;
            memcpy(udevr[i].name, name, strlen(name)+1);
            return 0;
        }
    }

    usize osz = udevrav;
    void* nudevr = realloc(udevr, sizeof(udrv_t) * (udevrav + 16));
    if (!nudevr) return -ENOMEM;

    memset(nudevr + osz, 0, sizeof(udrv_t) * 16);
    udevr = nudevr;
    udevrav += 16;

    udevr[osz].used = 1;
    udevr[osz].id = id;
    udevr[osz].flags = flags;
    udevr[osz].read = read;
    udevr[osz].write = write;
    memcpy(udevr[osz].name, name, strlen(name)+1);

    return 0;
}

int udevr_regdev(u16 drv, u16 id) {
    if (!udevr) return -ENOEXIST;
    if (udevr_devfs) {
        int ret = 0;

        for (usize i = 0; i < udevrav; i++) {
            if (udevr[i].used && udevr[i].id == drv) {
                char path[1024];
                snprintf(path, 1024, "/dev/%s%d", udevr[i].name, id);
                if ((ret = mknod(path, MKDEV(drv, id), 0644)) < 0) return ret;
                return 0;
            }
        }

        return -ENOEXIST;
    }

    return 0;
}

ssize udevr_read(int fd, void* buf, usize sz) {
    if (!udevr) return -ENOEXIST;
    struct fdinfo* info;
    int ret = 0;
    if ((ret = getfd(fd, &info)) < 0) {
        return ret;
    }

    if (info->type != FDTYPE_DEV) return -EINVAL;

    u32 maj = MAJOR(info->data.dev.rdev);
    u32 min = MINOR(info->data.dev.rdev);
    
    for (usize i = 0; i < udevrav; i++) {
        if (udevr[i].used && udevr[i].id == maj) {
            if (udevr[i].flags & UDEV_RD) {
                return udevr[i].read(min, buf, sz);
            } else {
                return -EACCESS;
            }
        }
    }

    return -ENOEXIST;
}

ssize udevr_write(int fd, void* buf, usize sz) {
    if (!udevr) return -ENOEXIST;
    struct fdinfo* info;
    int ret = 0;
    if ((ret = getfd(fd, &info)) < 0) {
        return ret;
    }

    if (info->type != FDTYPE_DEV) return -EINVAL;

    u32 maj = MAJOR(info->data.dev.rdev);
    u32 min = MINOR(info->data.dev.rdev);
    
    for (usize i = 0; i < udevrav; i++) {
        if (udevr[i].used && udevr[i].id == maj) {
            if (udevr[i].flags & UDEV_WR) {
                return udevr[i].write(min, buf, sz);
            } else {
                return -EACCESS;
            }
        }
    }

    return -ENOEXIST;
}