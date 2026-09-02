#include <core/std.h>
#include <core/errno.h>
#include <core/lock.h>
#include <lib/string.h>
#include <core/printf.h>
#include <core/liballoc.h>
#include <drivers/storage/block.h>
#include <drivers/storage/ata.h>
#include <drivers/storage/ahci.h>
#include <drivers/storage/usbmsd.h>
#include <drivers/storage/virtio_blk.h>

static struct blockdev* blkdevs = NULL;
static usize nblkdevsup = 0;
static usize nblkdevavl = 0;

static u32 nblkdevtyp[] = {
    [DRV_ATA] = 0,
    [DRV_AHCI] = 0,
    [DRV_USBMSD] = 0,
    [DRV_VIRTIO] = 0
};

static const char* blkdevnams[] = {
    [DRV_ATA] = "ata",
    [DRV_AHCI] = "ahci",
    [DRV_USBMSD] = "usbblk",
    [DRV_VIRTIO] = "virtblk"
};

ssize block_init() {
    blkdevs = malloc(sizeof(struct blockdev) * 8);
    
    nblkdevsup = 8;
    if (!blkdevs) {
        return -1;
    }

    ata_enumerate();
    ahci_enumerate();
    usbmsd_enumerate();
    virtio_blk_enumerate();

    return nblkdevavl;
}

usize block_getndevs() {
    return nblkdevavl;
}

usize block_getndevstyp(u8 type) {
    return nblkdevtyp[type];
}

struct blockdev* block_getdevs() {
    return blkdevs;
}

int block_getdevnam(const char* name, struct blockdev* dev) {
    for (usize i = 0; i < nblkdevavl; i++) {
        if (streq(blkdevs[i].name, name)) {
            memcpy(dev, &blkdevs[i], sizeof(*dev));
            return 0;
        }
    }
    return -ENOEXIST;
}

int block_getdevid(u64 id, struct blockdev* dev) {
    for (usize i = 0; i < nblkdevavl; i++) {
        if (blkdevs[i].id == id) {
            memcpy(dev, &blkdevs[i], sizeof(*dev));
            return 0;
        }
    }
    return -ENOEXIST;
}

int block_register(u8 type, u64 priv) {
    if (type != DRV_ATA && type != DRV_AHCI &&
        type != DRV_USBMSD && type != DRV_VIRTIO) {
            return -EINVAL;
    }

    if (nblkdevavl + 1 >= nblkdevsup) {
        struct blockdev* nbdevs = realloc(blkdevs, sizeof(struct blockdev) * (nblkdevsup + 4));
        if (!nbdevs) return -ENOMEM;
        blkdevs = nbdevs;
        nblkdevsup += 4;
    }

    usize blkid = nblkdevavl++;

    struct blockdev* dev = &blkdevs[blkid];
    dev->id = BLKDEV_ID(type, nblkdevtyp[type]++);
    dev->priv = priv;
    lock_init(&dev->lock);
    snprintf(dev->name, 128, "%s%llu", blkdevnams[type], BLKDEV_DEVNO(dev->id));
    serial_printf("Registered block device %s\n", dev->name);
    return 0;
}

int block_write(u64 id, const u8* buf, u32 lba, usize cnt) {
    struct blockdev dev;
    if (block_getdevid(id, &dev) < 0) return -ENOEXIST;

    int (*fn32)(u64 prv, u32 lba, u8* buf) = NULL;
    int (*fn64)(u64 prv, u64 lba, u8* buf) = NULL;

    if (BLKDEV_TYPE(dev.id) == DRV_ATA) {
        fn32 = ata_secwrite;
    } else if (BLKDEV_TYPE(dev.id) == DRV_AHCI) {
        fn64 = ahci_secwrite;
    } else if (BLKDEV_TYPE(dev.id) == DRV_USBMSD) {
        fn32 = usbmsd_secwrite;
    } else if (BLKDEV_TYPE(dev.id) == DRV_VIRTIO) {
        fn32 = virtio_blk_secwrite;
    } else {
        return -EINVAL;
    }

    lock_acquire(&dev.lock);

    int ret = 0;
    if (fn32) {
        for (usize i = 0; i < cnt; i++) {
            int v = fn32(dev.priv, (u32)(lba + i), (u8*)(buf + (i * 512)));
            if (v < 0) {
                ret = v;
                break;
            }
        }
    } else {
        for (usize i = 0; i < cnt; i++) {
            int v = fn64(dev.priv, (u64)(lba + i), (u8*)(buf + (i * 512)));
            if (v < 0) {
                ret = v;
                break;
            }
        }
    }

    lock_release(&dev.lock);

    return ret;
}

int block_read(u64 id, u8* buf, u32 lba, usize cnt) {
    struct blockdev dev;
    if (block_getdevid(id, &dev) < 0) return -ENOEXIST;

    int (*fn32)(u64 id, u32 lba, u8* buf) = NULL;
    int (*fn64)(u64 id, u64 lba, u8* buf) = NULL;
    
    if (BLKDEV_TYPE(dev.id) == DRV_ATA) {
        fn32 = ata_secread;
    } else if (BLKDEV_TYPE(dev.id) == DRV_AHCI) {
        fn64 = ahci_secread;
    } else if (BLKDEV_TYPE(dev.id) == DRV_USBMSD) {
        fn32 = usbmsd_secread;
    } else if (BLKDEV_TYPE(dev.id) == DRV_VIRTIO) {
        fn32 = virtio_blk_secread;
    } else {
        return -EINVAL;
    }

    lock_acquire(&dev.lock);

    int ret = 0;
    if (fn32) {
        for (usize i = 0; i < cnt; i++) {
            int v = fn32(dev.priv, (u32)(lba + i), (u8*)(buf + (i * 512)));
            if (v < 0) {
                ret = v;
                break;
            }
        }
    } else {
        for (usize i = 0; i < cnt; i++) {
            int v = fn64(dev.priv, (u64)(lba + i), (u8*)(buf + (i * 512)));
            if (v < 0) {
                ret = v;
                break;
            }
        }
    }

    lock_release(&dev.lock);

    return ret;
}