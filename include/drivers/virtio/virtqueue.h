#pragma once
#include <core/std.h>
#include <drivers/virtio/virtio.h>

/* VirtQueue Descriptor Flags */
#define VRING_DESC_F_NEXT      1
#define VRING_DESC_F_WRITE     2
#define VRING_DESC_F_INDIRECT  4

/* VirtQueue Descriptor */
typedef struct {
    u64 addr;
    u32 len;
    u16 flags;
    u16 next;
} __attribute__((packed)) vring_desc_t;

/* VirtQueue Available Ring */
typedef struct {
    u16 flags;
    u16 idx;
    u16 ring[];
} __attribute__((packed)) vring_avail_t;

/* VirtQueue Used Ring Element */
typedef struct {
    u32 id;
    u32 len;
} __attribute__((packed)) vring_used_elem_t;

/* VirtQueue Used Ring */
typedef struct {
    u16 flags;
    u16 idx;
    vring_used_elem_t ring[];
} __attribute__((packed)) vring_used_t;

/* In-memory representation of a VirtQueue */
typedef struct {
    virtio_dev_t* dev;
    u16 queue_index;
    u16 size;
    u16 free_count;
    u16 free_head;
    u16 last_used_idx;

    vring_desc_t* desc;
    vring_avail_t* avail;
    vring_used_t* used;

    u64 phys_base;
    usize page_count;
} virtqueue_t;

int virtqueue_init(virtio_dev_t* dev, u16 queue_index, virtqueue_t* vq);
void virtqueue_free(virtqueue_t* vq);

s32 virtqueue_alloc_desc(virtqueue_t* vq);
void virtqueue_free_desc(virtqueue_t* vq, u16 desc_idx);
void virtqueue_free_chain(virtqueue_t* vq, u16 head);

void virtqueue_submit_chain(virtqueue_t* vq, u16 head);
void virtqueue_kick(virtqueue_t* vq);
bool virtqueue_has_used(virtqueue_t* vq);
int virtqueue_poll_used(virtqueue_t* vq, u32* len_out, u32 timeout_spins);
