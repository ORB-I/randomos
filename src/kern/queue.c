#include <core/kqueue.h>
#include <core/liballoc.h>

kqueue_t* kqueue_init(usize maxsz) {
    if (maxsz == 0) return NULL;

    kqueue_t* queue = malloc(sizeof(kqueue_t));
    if (!queue) return NULL;
    
    queue->buf = malloc(maxsz);
    if (!queue->buf) {
        free(queue);
        return NULL;
    }

    queue->sz = maxsz;
    queue->head = 0;
    queue->tail = 0;
    queue->cnt = 0;

    return queue;
}

// data => data to queue
// datasz => size of data to queue
// returns how much data was actually queued
usize kqueue_enqueue(kqueue_t* queue, u8* data, usize datasz) {
    if (!queue || !data || datasz == 0) {
        return 0;
    }

    usize avail = queue->sz - queue->cnt;
    usize n = datasz < avail ? datasz : avail;

    for (usize i = 0; i < n; i++) {
        queue->buf[queue->head] = data[i];
        queue->head = (queue->head + 1) % queue->sz;
    }

    queue->cnt += n;
    return n;
}

// data => buffer to fill
// datasz => requested bytes of data
// returns how much data was actually put in the buffer
usize kqueue_dequeue(kqueue_t* queue, u8* data, usize datasz) {
    if (!queue || !data || datasz == 0) {
        return 0;
    }

    usize n = datasz < queue->cnt ? datasz : queue->cnt;

    for (usize i = 0; i < n; i++) {
        data[i] = queue->buf[queue->tail];
        queue->tail = (queue->tail + 1) % queue->sz;
    }

    queue->cnt -= n;
    return n;
}

usize kqueue_queued(kqueue_t* queue) {
    if (!queue) return 0;
    return queue->cnt;
}