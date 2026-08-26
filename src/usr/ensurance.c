#include <core/std.h>
#include <core/mem/vmm.h>

int okaddr(page_table_t* tbl, u64 addr, int write) {
    if (!tbl) return 0;

    u64 pml4e = tbl[PML4_IDX(addr)];
    if (!(pml4e & PAGE_PRESENT)) return 0;
    if (write && !(pml4e & PAGE_WRITE)) return 0;
    page_table_t* pdptv = (page_table_t*)(HHDM_START + (pml4e & ~0xFFFULL));

    u64 pdpte = pdptv[PDPT_IDX(addr)];
    if (!(pdpte & PAGE_PRESENT)) return 0;
    if (write && !(pdpte & PAGE_WRITE)) return 0;
    page_table_t* pdv = (page_table_t*)(HHDM_START + (pdpte & ~0xFFFULL));

    u64 pde = pdv[PD_IDX(addr)];
    if (!(pde & PAGE_PRESENT)) return 0;
    if (write && !(pde & PAGE_WRITE)) return 0;

    if (pde & PAGE_HUGE) return 1;

    page_table_t* ptv = (page_table_t*)(HHDM_START + (pde & ~0xFFFULL));

    u64 pte = ptv[PT_IDX(addr)];
    if (!(pte & PAGE_PRESENT)) return 0;
    if (write && !(pte & PAGE_WRITE)) return 0;

    return 1;
}

// ensures
// - not NULL
// - canonical address
// - mapped
// - if write op, can write
#include <drivers/display/serial.h>
int ensure_pointer(void* uptr, usize sz, int write) {
    if (!uptr) return 0;
    u64 addr = (u64)uptr;
    u64 cnon = addr >> 47;
    if (cnon != 0 && cnon != 0x1FFFF) return 0;
    

    for (usize i = 0; i < sz; i += 0x1000) {
        u64 pg = (addr + i) & ~(u64)0xFFF;
        if (!okaddr(vmm_cpml4v(), pg, write)) return 0;
    }

    return 1;
}

int ensure_string(char* str, usize maxsz, int write) {
    for (usize i = 0; i < maxsz; i++) {
        if (!ensure_pointer((void*)(str + i), i+1, write)) {
            return 0;
        }

        if (str[i] == '\0') {
            return 1;
        }
    }
    return 0;
}