#include <mem.h>

volatile int __liballoc_spl = 0;
int liballoc_lock() {
    while (__sync_lock_test_and_set(&__liballoc_spl, 1)) {
        asm volatile("pause" ::: "memory");
    }
    return 0;
}

int liballoc_unlock() {
    __sync_lock_release(&__liballoc_spl);
    return 0;
}

void* liballoc_alloc(int npgs) {
    return mmap(MMAP_ADDRANY, npgs);
}

int liballoc_free(void* addr, int npg) {
    return munmap(addr, npg);
}