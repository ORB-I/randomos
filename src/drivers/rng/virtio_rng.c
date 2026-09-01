#include <core/std.h>
#include <core/asmh.h>
#include <core/errno.h>
#include <core/printf.h>
#include <core/mem/pmm.h>
#include <core/mem/vmm.h>
#include <lib/string.h>
#include <drivers/virtio/virtio.h>
#include <drivers/virtio/virtqueue.h>
#include <drivers/rng/virtio_rng.h>

static virtio_dev_t rng_dev;
static virtqueue_t rng_vq;
static bool rng_initialized = false;

static u64 rng_dma_phys = 0;
static u8* rng_dma_virt = NULL;

int virtio_rng_init() {
    if (rng_initialized) return 0;

    if (virtio_find_pci_device(VIRTIO_DEV_RNG, &rng_dev) < 0) {
        return -ENOEXIST;
    }

    serial_printf("virtio-rng: Found entropy device at %02x:%02x.%d (iobase=0x%x)\n",
                  rng_dev.bus, rng_dev.slot, rng_dev.fn, rng_dev.iobase);

    /* Reset device */
    virtio_reset(&rng_dev);

    /* Acknowledge & Driver */
    virtio_set_status(&rng_dev, VIRTIO_STATUS_ACKNOWLEDGE);
    virtio_add_status(&rng_dev, VIRTIO_STATUS_DRIVER);

    /* Feature negotiation */
    virtio_set_features(&rng_dev, 0);

    /* Initialize VirtQueue 0 */
    if (virtqueue_init(&rng_dev, 0, &rng_vq) < 0) {
        serial_printf("virtio-rng: Failed to initialize virtqueue\n");
        virtio_set_status(&rng_dev, VIRTIO_STATUS_FAILED);
        return -EDISK;
    }

    /* Allocate DMA buffer page */
    rng_dma_phys = (u64)pmm_falloc(1);
    if (!rng_dma_phys) {
        serial_printf("virtio-rng: Failed to allocate DMA page\n");
        virtio_set_status(&rng_dev, VIRTIO_STATUS_FAILED);
        return -ENOMEM;
    }
    rng_dma_virt = (u8*)(HHDM_START + rng_dma_phys);
    memset(rng_dma_virt, 0, 4096);

    /* Driver OK */
    virtio_add_status(&rng_dev, VIRTIO_STATUS_DRIVER_OK);

    rng_initialized = true;
    serial_printf("virtio-rng: Entropy source active\n");
    return 0;
}

bool virtio_rng_available() {
    return rng_initialized;
}

int virtio_rng_read(u8* buf, usize len) {
    if (!rng_initialized || !buf || len == 0) {
        return -EINVAL;
    }

    usize bytes_read = 0;
    while (bytes_read < len) {
        usize chunk = len - bytes_read;
        if (chunk > 4096) chunk = 4096;

        s32 desc = virtqueue_alloc_desc(&rng_vq);
        if (desc < 0) return -EFULL;

        rng_vq.desc[desc].addr = rng_dma_phys;
        rng_vq.desc[desc].len = (u32)chunk;
        rng_vq.desc[desc].flags = VRING_DESC_F_WRITE;
        rng_vq.desc[desc].next = 0;

        virtqueue_submit_chain(&rng_vq, (u16)desc);
        virtqueue_kick(&rng_vq);

        u32 out_len = 0;
        int res = virtqueue_poll_used(&rng_vq, &out_len, 1000000);
        virtqueue_free_desc(&rng_vq, (u16)desc);

        if (res < 0 || out_len == 0) {
            return -ETIME;
        }

        memcpy(buf + bytes_read, rng_dma_virt, out_len);
        bytes_read += out_len;
    }

    return 0;
}
