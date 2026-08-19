#include <mem.h>

volatile int __liballoc_spl = 0;
int liballoc_lock() {
    asm volatile(
        "1: xchg %0, %1\n\t"
        "test %0, %0\n\t"
        "jz 2f\n\t"
        "pause\n\t"
        "jmp 1b\n\t"
        "2:"
        : "=r"(__liballoc_spl), "=m"(__liballoc_spl)
        : "0"(1), "m"(__liballoc_spl)
        : "memory"
    );
    return 0;
}

int liballoc_unlock() {
    asm volatile("xchg %0, %1" : "=r"(__liballoc_spl), "=m"(__liballoc_spl) : "0"(0), "m"(__liballoc_spl) : "memory");
    return 0;
}

void* liballoc_alloc(int npgs) {
    return mmap(MMAP_ADDRANY, npgs);
}

int liballoc_free(void* addr, int npg) {
    return munmap(addr, npg);
}