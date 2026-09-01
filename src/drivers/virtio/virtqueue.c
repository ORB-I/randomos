#include <core/std.h>
#include <core/asmh.h>
#include <core/errno.h>
#include <core/printf.h>
#include <core/mem/pmm.h>
#include <core/mem/vmm.h>
#include <lib/string.h>
#include <drivers/virtio/virtqueue.h>

int virtqueue_init(virtio_dev_t* dev, u16 queue_index, virtqueue_t* vq) {
    if (!dev || !vq) return -EINVAL;

    memset(vq, 0, sizeof(virtqueue_t));
    vq->dev = dev;
    vq->queue_index = queue_index;

    /* Select queue */
    outw(dev->iobase + VIRTIO_REG_QUEUE_SELECT, queue_index);

    /* Read queue size */
    u16 size = inw(dev->iobase + VIRTIO_REG_QUEUE_SIZE);
    if (size == 0 || size > 32768) {
        return -ENOEXIST;
    }
    vq->size = size;

    /* Compute memory layout per legacy VirtIO spec (4096-byte alignment for used ring) */
    usize desc_sz = (usize)size * sizeof(vring_desc_t);
    usize avail_sz = sizeof(u16) * (3 + size);
    usize avail_end = desc_sz + avail_sz;
    usize used_offset = (avail_end + 4095) & ~4095;
    usize used_sz = sizeof(u16) * 3 + sizeof(vring_used_elem_t) * size;
    usize total_sz = used_offset + used_sz;
    usize page_cnt = (total_sz + 4095) / 4096;

    vq->page_count = page_cnt;
    vq->phys_base = (u64)pmm_falloc(page_cnt);
    if (!vq->phys_base) {
        return -ENOMEM;
    }

    u8* virt = (u8*)(HHDM_START + vq->phys_base);
    memset(virt, 0, page_cnt * 4096);

    vq->desc = (vring_desc_t*)virt;
    vq->avail = (vring_avail_t*)(virt + desc_sz);
    vq->used = (vring_used_t*)(virt + used_offset);

    /* Initialize free descriptor list */
    for (u16 i = 0; i < size; i++) {
        vq->desc[i].next = (i + 1 < size) ? (i + 1) : 0;
        vq->desc[i].flags = 0;
        vq->desc[i].addr = 0;
        vq->desc[i].len = 0;
    }
    vq->free_head = 0;
    vq->free_count = size;
    vq->last_used_idx = 0;

    /* Program PFN (Page Frame Number) into queue address register */
    outl(dev->iobase + VIRTIO_REG_QUEUE_ADDRESS, (u32)(vq->phys_base >> 12));

    return 0;
}

void virtqueue_free(virtqueue_t* vq) {
    if (!vq || !vq->phys_base) return;

    /* Unlink from device */
    outw(vq->dev->iobase + VIRTIO_REG_QUEUE_SELECT, vq->queue_index);
    outl(vq->dev->iobase + VIRTIO_REG_QUEUE_ADDRESS, 0);

    pmm_ffree((void*)vq->phys_base, vq->page_count);
    memset(vq, 0, sizeof(virtqueue_t));
}

s32 virtqueue_alloc_desc(virtqueue_t* vq) {
    if (!vq || vq->free_count == 0) return -1;

    u16 idx = vq->free_head;
    vq->free_head = vq->desc[idx].next;
    vq->free_count--;

    vq->desc[idx].flags = 0;
    vq->desc[idx].next = 0;
    vq->desc[idx].addr = 0;
    vq->desc[idx].len = 0;

    return (s32)idx;
}

void virtqueue_free_desc(virtqueue_t* vq, u16 desc_idx) {
    if (!vq || desc_idx >= vq->size) return;

    vq->desc[desc_idx].next = vq->free_head;
    vq->desc[desc_idx].flags = 0;
    vq->free_head = desc_idx;
    vq->free_count++;
}

void virtqueue_free_chain(virtqueue_t* vq, u16 head) {
    if (!vq || head >= vq->size) return;

    u16 curr = head;
    for (;;) {
        u16 next = vq->desc[curr].next;
        bool has_next = (vq->desc[curr].flags & VRING_DESC_F_NEXT) != 0;
        virtqueue_free_desc(vq, curr);
        if (!has_next) break;
        curr = next;
    }
}

void virtqueue_submit_chain(virtqueue_t* vq, u16 head) {
    if (!vq) return;

    u16 avail_slot = vq->avail->idx % vq->size;
    vq->avail->ring[avail_slot] = head;
    asm volatile("" ::: "memory");
    vq->avail->idx++;
    asm volatile("" ::: "memory");
}

void virtqueue_kick(virtqueue_t* vq) {
    if (!vq || !vq->dev) return;
    asm volatile("" ::: "memory");
    outw(vq->dev->iobase + VIRTIO_REG_QUEUE_NOTIFY, vq->queue_index);
}

bool virtqueue_has_used(virtqueue_t* vq) {
    if (!vq) return false;
    asm volatile("" ::: "memory");
    return vq->last_used_idx != vq->used->idx;
}

int virtqueue_poll_used(virtqueue_t* vq, u32* len_out, u32 timeout_spins) {
    if (!vq) return -EINVAL;

    for (u32 spin = 0; spin < timeout_spins; spin++) {
        asm volatile("" ::: "memory");
        if (vq->last_used_idx != vq->used->idx) {
            u16 used_slot = vq->last_used_idx % vq->size;
            vring_used_elem_t elem = vq->used->ring[used_slot];
            if (len_out) *len_out = elem.len;
            vq->last_used_idx++;
            return (int)elem.id;
        }
        asm volatile("pause");
    }

    return -ETIME;
}
