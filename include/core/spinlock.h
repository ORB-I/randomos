#pragma once
#include <stdatomic.h>
#include <core/std.h>

typedef struct {
    atomic_int lock;
} spinlock_t;

#define SPINLOCK_INIT { 0 }

__no_protect __must_inline static inline void spinlock_acquire(spinlock_t* lock) {
    while (atomic_exchange_explicit(&lock->lock, 1, memory_order_acquire) != 0) {
        asm volatile("pause" ::: "memory");
    }
}

__no_protect __must_inline static inline void spinlock_release(spinlock_t* lock) {
    atomic_store_explicit(&lock->lock, 0, memory_order_release);
}
