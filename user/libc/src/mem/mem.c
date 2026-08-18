#include <mem.h>
#include <sys/syscall.h>

void* mmap(void* addr, u64 npages) {
    return (void*)__syscall2(SYS_MMAP, (u64)addr, npages);
}

int munmap(void* addr, u64 npages) {
    return __syscall2(SYS_MUNMAP, (u64)addr, npages);
}