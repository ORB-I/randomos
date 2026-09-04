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

extern u64 __uvmm_map_high__;
extern u64 __uvmm_map_low__;
u64 __alloc_anoncurrent = 0;

#define PAGE_WRITE    (1ULL << 1)
#define MAP_ANYVIRT   (1ULL << 62)

void* liballoc_alloc(int npgs) {
    u64 bytes = npgs * 4096;
    if (__alloc_anoncurrent + bytes > __uvmm_map_high__) {
        return NULL;
    }
    u64 vaddr = __alloc_anoncurrent;
    __alloc_anoncurrent += bytes;

    return mmap((void*)vaddr, 0, npgs, MAP_ANYVIRT | PAGE_WRITE);
}

int liballoc_free(void* addr, int npg) {
    return munmap(addr, npg, 0);
}