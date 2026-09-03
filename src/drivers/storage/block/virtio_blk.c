#include <core/std.h>
#include <core/asmh.h>
#include <core/errno.h>
#include <core/kprint.h>
#include <core/liballoc.h>
#include <core/mem/pmm.h>
#include <core/mem/vmm.h>
#include <lib/string.h>
#include <drivers/virtio/virtio.h>
#include <drivers/virtio/virtqueue.h>
#include <drivers/storage/block/virtio_blk.h>
#include <drivers/storage/block/block.h>

#define DMA_HDR_OFF    0
#define DMA_STATUS_OFF 64
#define DMA_BUF_OFF    128

void virtio_blk_enumerate() {
    for (u32 bus = 0; bus < 256; bus++) {
        for (u32 slot = 0; slot < 32; slot++) {
            u8 maxfn = virtio_pci_maxfn(bus, slot);
            if (!maxfn) continue;
            for (u32 fn = 0; fn < maxfn; fn++) {
                if (virtio_pci_isfunc(bus, slot, fn, VIRTIO_DEV_BLOCK)) {
                    virtblk_dev_t* dev = malloc(sizeof(*dev));
                    if (!dev) return;

                    if (virtio_pci_initfn(&dev->vd, bus, slot, fn, 0) < 0) {
                        free(dev);
                        continue;
                    }

                    // ack dev and set driver
                    virtio_reset(&dev->vd);
                    virtio_set_status(&dev->vd, VIRTIO_STATUS_ACKNOWLEDGE);
                    virtio_add_status(&dev->vd, VIRTIO_STATUS_DRIVER);

                    virtio_get_features(&dev->vd);
                    virtio_set_features(&dev->vd, 0);

                    if (virtqueue_init(&dev->vd, 0, &dev->vq) < 0) {
                        virtio_set_status(&dev->vd, VIRTIO_STATUS_FAILED);
                        free(dev);
                        continue;
                    }

                    dev->dma_phys = (u64)pmm_falloc(1);
                    if (!dev->dma_phys) {
                        virtio_set_status(&dev->vd, VIRTIO_STATUS_FAILED);
                        free(dev);
                        return;
                    }

                    dev->dma_virt = (u8*)(HHDM_START + dev->dma_phys);
                    memset(dev->dma_virt, 0, 4096);

                    if (block_register(DRV_VIRTIO, (u64)dev) < 0) {
                        virtio_set_status(&dev->vd, VIRTIO_STATUS_FAILED);
                        free(dev);
                        return;
                    }

                    virtio_add_status(&dev->vd, VIRTIO_STATUS_DRIVER_OK);
                    dev->cap = virtio_read_config64(&dev->vd, 0);
                }
            }
        }
    }
}

static int virtio_blk_transfer(virtblk_dev_t* dev, u32 type, u64 lba, u8* buf) {
    virtio_blk_req_hdr_t* hdr = (virtio_blk_req_hdr_t*)(dev->dma_virt + DMA_HDR_OFF);
    volatile u8* status_ptr = (volatile u8*)(dev->dma_virt + DMA_STATUS_OFF);
    u8* dma_buf = dev->dma_virt + DMA_BUF_OFF;

    hdr->type = type;
    hdr->reserved = 0;
    hdr->sector = lba;
    *status_ptr = 0xFF;

    if (type == VIRTIO_BLK_T_OUT) {
        memcpy(dma_buf, buf, 512);
    }

    /* Allocate 3 descriptors */
    s32 d0 = virtqueue_alloc_desc(&dev->vq);
    s32 d1 = virtqueue_alloc_desc(&dev->vq);
    s32 d2 = virtqueue_alloc_desc(&dev->vq);

    if (d0 < 0 || d1 < 0 || d2 < 0) {
        if (d0 >= 0) virtqueue_free_desc(&dev->vq, (u16)d0);
        if (d1 >= 0) virtqueue_free_desc(&dev->vq, (u16)d1);
        if (d2 >= 0) virtqueue_free_desc(&dev->vq, (u16)d2);
        return -EFULL;
    }

    /* Descriptor 0: Request Header (Device read-only) */
    dev->vq.desc[d0].addr = dev->dma_phys + DMA_HDR_OFF;
    dev->vq.desc[d0].len = sizeof(virtio_blk_req_hdr_t);
    dev->vq.desc[d0].flags = VRING_DESC_F_NEXT;
    dev->vq.desc[d0].next = (u16)d1;

    /* Descriptor 1: Buffer (Device write-only for IN/read, Device read-only for OUT/write) */
    dev->vq.desc[d1].addr = dev->dma_phys + DMA_BUF_OFF;
    dev->vq.desc[d1].len = 512;
    dev->vq.desc[d1].flags = ((type == VIRTIO_BLK_T_IN) ? VRING_DESC_F_WRITE : 0) | VRING_DESC_F_NEXT;
    dev->vq.desc[d1].next = (u16)d2;

    /* Descriptor 2: Status Byte (Device write-only) */
    dev->vq.desc[d2].addr = dev->dma_phys + DMA_STATUS_OFF;
    dev->vq.desc[d2].len = 1;
    dev->vq.desc[d2].flags = VRING_DESC_F_WRITE;
    dev->vq.desc[d2].next = 0;

    /* Submit and notify */
    virtqueue_submit_chain(&dev->vq, (u16)d0);
    virtqueue_kick(&dev->vq);

    /* Poll for completion */
    int res = virtqueue_poll_used(&dev->vq, NULL, 10000000);

    /* Free descriptor chain */
    virtqueue_free_chain(&dev->vq, (u16)d0);

    if (res < 0) {
        kprint("virtio-blk: Request timed out\n");
        return -ETIME;
    }

    if (*status_ptr != VIRTIO_BLK_S_OK) {
        kprint("virtio-blk: I/O error status=%d\n", *status_ptr);
        return -EDISK;
    }

    if (type == VIRTIO_BLK_T_IN) {
        memcpy(buf, dma_buf, 512);
    }

    return 0;
}

int virtio_blk_secread(u64 id, u32 lba, u8* buf) {
    virtblk_dev_t* dev = (virtblk_dev_t*)id;
    return virtio_blk_transfer(dev, VIRTIO_BLK_T_IN, (u64)lba, buf);
}

int virtio_blk_secwrite(u64 id, u32 lba, u8* buf) {
    virtblk_dev_t* dev = (virtblk_dev_t*)id;
    return virtio_blk_transfer(dev, VIRTIO_BLK_T_OUT, (u64)lba, buf);
}

u64 virtio_blk_get_capacity(virtblk_dev_t* dev) {
    return dev->cap;
}
