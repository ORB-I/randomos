#include <core/liballoc.h>
#include <core/mem/vmm.h>
#include <core/lock.h>

spinlock_t __liballoc_spl;
bool       __liballoc_spl_inited = false;
int liballoc_lock() {
    if (!__liballoc_spl_inited) {
        spl_init(&__liballoc_spl);
        __liballoc_spl_inited = true;
    }

    spl_lock(&__liballoc_spl);
    return 0;
}

int liballoc_unlock() {
    spl_unlock(&__liballoc_spl);
    return 0;
}

void* liballoc_alloc(int npg) {
    return vmm_map_pages(vmm_cpml4v(), 0, 0, npg, MAP_ANYPHYS | MAP_ANYVIRT | PAGE_WRITE);
}

int liballoc_free(void* addr, int npg) {
    vmm_unmap_pages(vmm_cpml4v(), (u64)addr, npg, 0);
    return 0;
}