#include <core/lock.h>

void spl_init(spinlock_t* spl) {
    spl->__lkst = 0;
}

void spl_lock(spinlock_t* spl) {
    asm volatile("cli" ::: "memory");
    while (__sync_lock_test_and_set(&spl->__lkst, 1)) {
        asm volatile(
            "sti\n\t"
            "pause\n\t"
            "cli" 
            ::: "memory"
        );
    }
}

void spl_unlock(spinlock_t* spl) {
    __sync_lock_release(&spl->__lkst);
    asm volatile("sti" ::: "memory");
}