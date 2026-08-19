#include <core/lock.h>

void spl_init(spinlock_t* spl) {
    spl->__lkst = 0;
}

void spl_lock(spinlock_t* spl) {
    asm volatile("cli" ::: "memory");
    asm volatile(
        "1: xchg %0, %1\n\t"
        "test %0, %0\n\t"
        "jz 2f\n\t"
        "3:\n\t"
        "sti\n\t"
        "pause\n\t"
        "cli\n\t"
        "jmp 1b\n\t"
        "2:"
        : "=r"(spl->__lkst), "=m"(spl->__lkst)
        : "0"(1), "m"(spl->__lkst)
        : "memory"
    );
}

void spl_unlock(spinlock_t* spl) {
    asm volatile("xchg %0, %1" : "=r"(spl->__lkst), "=m"(spl->__lkst) : "0"(0), "m"(spl->__lkst) : "memory");
    asm volatile("sti" ::: "memory");
}