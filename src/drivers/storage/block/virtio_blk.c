#include <core/std.h>
#include <core/asmh.h>
#include <core/errno.h>
#include <core/printf.h>
#include <core/mem/pmm.h>
#include <core/mem/vmm.h>
#include <lib/string.h>
#include <drivers/virtio/virtio.h>
#include <drivers/virtio/virtqueue.h>
#include <drivers/storage/virtio_blk.h>

static virtio_dev_t blk_dev;
static virtqueue_t blk_vq;
static bool blk_initialized = false;
static u64 blk_capacity = 0;

/* DMA bounce buffer allocated via PMM */
static u64 blk_dma_phys = 0;
static u8* blk_dma_virt = NULL;

#define DMA_HDR_OFF    0
#define DMA_STATUS_OFF 64
#define DMA_BUF_OFF    128

int virtio_blk_init() {
    if (blk_initialized) return 1;

    if (virtio_find_pci_device(VIRTIO_DEV_BLOCK, &blk_dev, 0) < 0) {
        return -ENOEXIST;
    }

    serial_printf("virtio-blk: Found device at %02x:%02x.%d (iobase=0x%x, irq=%d)\n",
                  blk_dev.bus, blk_dev.slot, blk_dev.fn, blk_dev.iobase, blk_dev.irq);

    /* Reset device */
    virtio_reset(&blk_dev);

    /* Acknowledge device & indicate driver */
    virtio_set_status(&blk_dev, VIRTIO_STATUS_ACKNOWLEDGE);
    virtio_add_status(&blk_dev, VIRTIO_STATUS_DRIVER);

    /* Feature negotiation */
    u32 features = virtio_get_features(&blk_dev);
    (void)features;
    virtio_set_features(&blk_dev, 0);

    /* Initialize VirtQueue 0 */
    if (virtqueue_init(&blk_dev, 0, &blk_vq) < 0) {
        serial_printf("virtio-blk: Failed to initialize request virtqueue\n");
        virtio_set_status(&blk_dev, VIRTIO_STATUS_FAILED);
        return -EDISK;
    }

    /* Allocate DMA page for request header, status byte, and sector bounce buffer */
    blk_dma_phys = (u64)pmm_falloc(1);
    if (!blk_dma_phys) {
        serial_printf("virtio-blk: Failed to allocate DMA page\n");
        virtio_set_status(&blk_dev, VIRTIO_STATUS_FAILED);
        return -ENOMEM;
    }
    blk_dma_virt = (u8*)(HHDM_START + blk_dma_phys);
    memset(blk_dma_virt, 0, 4096);

    /* Driver OK */
    virtio_add_status(&blk_dev, VIRTIO_STATUS_DRIVER_OK);

    /* Read capacity (in 512-byte sectors) */
    blk_capacity = virtio_read_config64(&blk_dev, 0);
    serial_printf("virtio-blk: Capacity %llu sectors (%llu MB)\n",
                  blk_capacity, (blk_capacity * 512) / (1024 * 1024));

    blk_initialized = true;
    return 1;
}

static int virtio_blk_transfer(u32 type, u64 lba, u8* buf) {
    if (!blk_initialized) return -EINVAL;

    virtio_blk_req_hdr_t* hdr = (virtio_blk_req_hdr_t*)(blk_dma_virt + DMA_HDR_OFF);
    volatile u8* status_ptr = (volatile u8*)(blk_dma_virt + DMA_STATUS_OFF);
    u8* dma_buf = blk_dma_virt + DMA_BUF_OFF;

    hdr->type = type;
    hdr->reserved = 0;
    hdr->sector = lba;
    *status_ptr = 0xFF;

    if (type == VIRTIO_BLK_T_OUT) {
        memcpy(dma_buf, buf, 512);
    }

    /* Allocate 3 descriptors */
    s32 d0 = virtqueue_alloc_desc(&blk_vq);
    s32 d1 = virtqueue_alloc_desc(&blk_vq);
    s32 d2 = virtqueue_alloc_desc(&blk_vq);

    if (d0 < 0 || d1 < 0 || d2 < 0) {
        if (d0 >= 0) virtqueue_free_desc(&blk_vq, (u16)d0);
        if (d1 >= 0) virtqueue_free_desc(&blk_vq, (u16)d1);
        if (d2 >= 0) virtqueue_free_desc(&blk_vq, (u16)d2);
        return -EFULL;
    }

    /* Descriptor 0: Request Header (Device read-only) */
    blk_vq.desc[d0].addr = blk_dma_phys + DMA_HDR_OFF;
    blk_vq.desc[d0].len = sizeof(virtio_blk_req_hdr_t);
    blk_vq.desc[d0].flags = VRING_DESC_F_NEXT;
    blk_vq.desc[d0].next = (u16)d1;

    /* Descriptor 1: Buffer (Device write-only for IN/read, Device read-only for OUT/write) */
    blk_vq.desc[d1].addr = blk_dma_phys + DMA_BUF_OFF;
    blk_vq.desc[d1].len = 512;
    blk_vq.desc[d1].flags = ((type == VIRTIO_BLK_T_IN) ? VRING_DESC_F_WRITE : 0) | VRING_DESC_F_NEXT;
    blk_vq.desc[d1].next = (u16)d2;

    /* Descriptor 2: Status Byte (Device write-only) */
    blk_vq.desc[d2].addr = blk_dma_phys + DMA_STATUS_OFF;
    blk_vq.desc[d2].len = 1;
    blk_vq.desc[d2].flags = VRING_DESC_F_WRITE;
    blk_vq.desc[d2].next = 0;

    /* Submit and notify */
    virtqueue_submit_chain(&blk_vq, (u16)d0);
    virtqueue_kick(&blk_vq);

    /* Poll for completion */
    int res = virtqueue_poll_used(&blk_vq, NULL, 10000000);

    /* Free descriptor chain */
    virtqueue_free_chain(&blk_vq, (u16)d0);

    if (res < 0) {
        printf("virtio-blk: Request timed out\n");
        return -ETIME;
    }

    if (*status_ptr != VIRTIO_BLK_S_OK) {
        printf("virtio-blk: I/O error status=%d\n", *status_ptr);
        return -EDISK;
    }

    if (type == VIRTIO_BLK_T_IN) {
        memcpy(buf, dma_buf, 512);
    }

    return 0;
}

int virtio_blk_secread(u8 id, u32 lba, u8* buf) {
    (void)id;
    return virtio_blk_transfer(VIRTIO_BLK_T_IN, (u64)lba, buf);
}

int virtio_blk_secwrite(u8 id, u32 lba, u8* buf) {
    (void)id;
    return virtio_blk_transfer(VIRTIO_BLK_T_OUT, (u64)lba, buf);
}

u64 virtio_blk_get_capacity() {
    return blk_capacity;
}
