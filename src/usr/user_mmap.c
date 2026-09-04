#include <core/mem/vmm.h>
#include <core/std.h>
#include <core/errno.h>

extern u8 current_pid;

void* user_mmap(page_table_t* uasp, void* reqaddr, u64 phys, u64 npages, u64 flags) {
    if (npages == 0) return NULL;

    u64 addr = (u64)reqaddr;
    if (addr >= USER_END) return NULL;
    return vmm_map_pages(uasp, addr, phys, npages, flags | MAP_ANYPHYS | PAGE_USER);
}

int user_munmap(page_table_t* uasp, void* addr, u64 npages, u64 flags) {
    if (addr == 0 || (u64)addr >= USER_END) {
        return -EINVAL;
    }
    vmm_unmap_pages(uasp, (u64)addr, npages, flags);
    return 0;
}

int user_mprotect(page_table_t *uasp, void *addr, u64 npgs, u64 flgs) {
    if (addr == 0 || (u64)addr >= USER_END) {
        return -EINVAL;
    }

    return vmm_setflgs(uasp, (u64)addr, npgs, flgs);
}