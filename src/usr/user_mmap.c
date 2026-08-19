#include <core/mem/vmm.h>
#include <core/std.h>

typedef struct {
    u64 vaddr_base;
    u64 vaddr_end;
    u64 pgcnt;
} vmm_range_t;

extern vmm_range_t vmm_umapr;

void* user_mmap(page_table_t* uasp, void* reqaddr, u64 npages) {
    if (reqaddr == 0) {
        return vmm_map_pages(uasp, 0, 0, npages, MAP_ANYPHYS | MAP_ANYVIRT | PAGE_USER | PAGE_WRITE);
    } else {
        if (!vmm_rangeinusrmap((u64)reqaddr, npages)) return NULL;
        return vmm_map_pages(uasp, (u64)reqaddr, 0, npages, MAP_ANYPHYS | PAGE_USER | PAGE_WRITE);
    }
}

int user_munmap(page_table_t* uasp, void* addr, u64 npages) {
    if (addr == 0) {
        return -1;
    }
    if (!vmm_rangeinusrmap((u64)addr, npages) &&
        ((u64)addr < HEAP_START || (u64)addr + npages * 4096 > HEAP_END)) {
        return -1;
    }
    vmm_unmap_pages(uasp, (u64)addr, npages, 0);
    return 0;
}