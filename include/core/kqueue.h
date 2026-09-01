#pragma once
#include <core/std.h>

typedef struct {
    u8* buf;
    usize sz;
    u64 head;
    u8 tail;
    usize cnt;
} kqueue_t;

kqueue_t* kqueue_init(usize maxsz);
usize kqueue_enqueue(kqueue_t* queue, u8* data, usize datasz);
usize kqueue_dequeue(kqueue_t* queue, u8* data, usize datasz);
usize kqueue_queued(kqueue_t* queue);