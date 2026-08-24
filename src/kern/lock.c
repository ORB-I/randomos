#include <core/lock.h>
#include <core/std.h>

void spl_init(spinlock_t* spl) {
    spl->__lkst = 0;
    spl->__flags = 0;
}

void spl_lock(spinlock_t* spl) {
    u64 flags;
    // remember the caller's IF state; the lock must not silently turn
    // interrupts on in code that entered with them off (syscall paths,
    // irq handlers, ...)
    asm volatile("pushfq\n\tpopq %0\n\tcli" : "=r"(flags) :: "memory");
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
    spl->__flags = flags;
}

void spl_unlock(spinlock_t* spl) {
    u64 flags = spl->__flags;
    asm volatile("xchg %0, %1" : "=r"(spl->__lkst), "=m"(spl->__lkst) : "0"(0), "m"(spl->__lkst) : "memory");
    // restore the acquirer's interrupt state instead of blindly sti-ing
    asm volatile("pushq %0\n\tpopfq" :: "r"(flags) : "memory");
}