#include <core/liballoc.h>
#include <core/mem/vmm.h>
#include <core/lock.h>

void* liballoc_alloc(int npg) {
    return vmm_map_pages(vmm_cpml4v(), 0, 0, npg, MAP_ANYPHYS | MAP_ANYVIRT | PAGE_WRITE);
}

int liballoc_free(void* addr, int npg) {
    vmm_unmap_pages(vmm_cpml4v(), (u64)addr, npg, 0);
    return 0;
}