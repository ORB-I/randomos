#include <mem.h>
#include <sys/syscall.h>

void* mmap(void* addr, u64 phys, u64 npages, u64 flags) {
    return (void*)__syscall4(SYS_MMAP, (u64)addr, phys, npages, flags);
}

int munmap(void* addr, u64 npages, usize flags) {
    return __syscall3(SYS_MUNMAP, (u64)addr, npages, flags);
}