#include <core/mem/vmm.h>
#include <core/std.h>

extern u8 current_pid;

void* user_mmap(page_table_t* uasp, void* reqaddr, u64 npages) {
    if (npages == 0) return NULL;

    if (reqaddr == 0) {
        u64 alloc_bytes = npages * 4096;
        if (vmm_umapr[current_pid].vaddr_curr + alloc_bytes > vmm_umapr[current_pid].vaddr_end) {
            return NULL;
        }
        u64 vaddr = vmm_umapr[current_pid].vaddr_curr;
        vmm_umapr[current_pid].vaddr_curr += alloc_bytes;

        return vmm_map_pages(uasp, vaddr, 0, npages, MAP_ANYPHYS | PAGE_USER | PAGE_WRITE);
    } else {
        u64 addr = (u64)reqaddr;
        if (!vmm_rangeinusrmap(addr, npages)) return NULL;
        return vmm_map_pages(uasp, addr, 0, npages, MAP_ANYPHYS | PAGE_USER | PAGE_WRITE);
    }
}

int user_munmap(page_table_t* uasp, void* addr, u64 npages) {
    if (addr == 0 || !vmm_rangeinusrmap((u64)addr, npages)) {
        return -1;
    }
    vmm_unmap_pages(uasp, (u64)addr, npages, 0);
    return 0;
}