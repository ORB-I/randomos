#pragma once
#include <stdatomic.h>
#include <core/std.h>

typedef struct {
    atomic_int lock;
} lock_t;

__attribute__((no_stack_protector))
__attribute__((always_inline))
static inline void lock_init(lock_t* lk) {
    lk->lock = 0;
}

__attribute__((no_stack_protector))
__attribute__((always_inline))
static inline void lock_acquire(lock_t* lk, u64* flags) {
    asm volatile(
        "pushfq\n\t"
        "pop %0\n\t"
        "cli"
        : "=r"(*flags)
        :: "memory", "cc"
    );
    while (atomic_exchange_explicit(&lk->lock, 1, memory_order_acquire) != 0) {
        asm volatile("pause");
    }
}

__attribute__((no_stack_protector))
__attribute__((always_inline))
static inline void lock_release(lock_t* lk, u64* flags) {
    atomic_store_explicit(&lk->lock, 0, memory_order_release);
    asm volatile(
        "push %0\n\t"
        "popfq"
        :: "r"(*flags)
        : "cc", "memory"
    );
}