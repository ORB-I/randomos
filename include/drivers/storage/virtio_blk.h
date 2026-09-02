#pragma once
#include <core/std.h>
#include <drivers/virtio/virtio.h>
#include <drivers/virtio/virtqueue.h>

/* VirtIO Block Request Types */
#define VIRTIO_BLK_T_IN           0  /* Read */
#define VIRTIO_BLK_T_OUT          1  /* Write */
#define VIRTIO_BLK_T_FLUSH        4  /* Flush */
#define VIRTIO_BLK_T_GET_ID       8  /* Get Device ID */

/* VirtIO Block Status */
#define VIRTIO_BLK_S_OK           0
#define VIRTIO_BLK_S_IOERR        1
#define VIRTIO_BLK_S_UNSUPP       2

/* VirtIO Block Request Header */
typedef struct {
    u32 type;
    u32 reserved;
    u64 sector;
} __attribute__((packed)) virtio_blk_req_hdr_t;

/* VirtIO Block Configuration in PCI space */
typedef struct {
    u64 capacity; /* In 512-byte sectors */
    u32 size_max;
    u32 seg_max;
    struct {
        u16 cylinders;
        u8 heads;
        u8 sectors;
    } geometry;
    u32 blk_size;
} __attribute__((packed)) virtio_blk_config_t;

typedef struct {
    virtio_dev_t vd;
    virtqueue_t vq;
    u64 cap;
    u64 dma_phys;
    u8* dma_virt;
} virtblk_dev_t;

void virtio_blk_enumerate();
int virtio_blk_secread(u64 id, u32 lba, u8* buf);
int virtio_blk_secwrite(u64 id, u32 lba, u8* buf);
u64 virtio_blk_get_capacity(virtblk_dev_t* dev);
