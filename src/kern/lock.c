#include <core/lock.h>
#include <core/std.h>
#include <stdatomic.h>

void lock_init(lock_t* lk) {
    lk->lock = 0;
    lk->rflags = 0;
}

void lock_acquire(lock_t* lk) {
    while (atomic_exchange_explicit(&lk->lock, 1, memory_order_acquire) != 0) {
        asm volatile("pause");
    }

    asm volatile(
        "pushfq\n\t"
        "pop %0"
        : "=r"(lk->rflags)
        :: "memory"
    );

    asm volatile("cli");
}

void lock_release(lock_t* lk) {
    atomic_store_explicit(&lk->lock, 0, memory_order_release);
    asm volatile(
        "push %0\n\t"
        "popfq"
        :: "r"(lk->rflags)
        : "cc", "memory"
    );
}