#pragma once
#include <core/std.h>
#include <core/lock.h>

#define DRV_ATA    0x01
#define DRV_AHCI   0x02
#define DRV_USBMSD 0x03
#define DRV_VIRTIO 0x04

#define BLKDEV_TYPE(ID)  (((ID) >> 24) & 0xFF)
#define BLKDEV_DEVNO(ID) ((ID) & 0x00FFFFFF)
#define BLKDEV_ID(TYPE, DEVNO) (((u64)(TYPE) << 24) | (u64)(DEVNO))

struct blockdev {
    char name[128];
    u32 id;
    u64 priv;
    lock_t lock;
};

ssize block_init();
usize block_getndevs();
usize block_getndevstyp(u8 type);
struct blockdev* block_getdevs();
int block_getdevnam(const char* name, struct blockdev* dev);
int block_getdevid(u64 id, struct blockdev* dev);
int block_write(u64 id, const u8* buf, u32 lba, usize cnt);
int block_read(u64 id, u8* buf, u32 lba, usize cnt);
int block_register(u8 type, u64 priv);