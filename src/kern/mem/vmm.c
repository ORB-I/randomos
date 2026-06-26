#include <core/limreqs.h>
#include <core/mem/vmm.h>
#include <core/mem/pmm.h>
#include <lib/string.h>

extern u64 hhdm_offset;
static page_table_t* pml4 = NULL;
static u64 lhhdm_off = 0;

extern u64 _kernel_end;
extern u64 _kernel_start;

typedef struct vmm_region {
    u64 vaddr_base;
    u64 page_count;
    u8 is_free;
    struct vmm_region* next;
} vmm_region_t;

#define MAX_VNODES 1024
static vmm_region_t* vmmr_head  = NULL;
static vmm_region_t vmmr_nodes[MAX_VNODES] = {0};

static vmm_region_t* allocvmmr() {
    for (int i = 0; i < MAX_VNODES; i++) {
        if (vmmr_nodes[i].vaddr_base == 0 && vmmr_nodes[i].page_count == 0 && !vmmr_nodes[i].is_free) {
            return &vmmr_nodes[i];
        }
    }
    return NULL; 
}

u64 vmm_ffreer(size_t pgcnt) {
    vmm_region_t* curr = vmmr_head;
    while (curr) {
        if (curr->is_free && curr->page_count >= pgcnt) {
            if (curr->page_count > pgcnt) {
                vmm_region_t* nblk = allocvmmr();
                if (!nblk) return 0;

                nblk->vaddr_base = curr->vaddr_base + (pgcnt * 4096);
                nblk->page_count = curr->page_count - pgcnt;
                nblk->is_free = 1;
                nblk->next = curr->next;

                curr->page_count = pgcnt;
                curr->next = nblk;
            }
            curr->is_free = 0;
            return curr->vaddr_base;
        }
        curr = curr->next;
    }
    return 0;
}

static page_table_t* alloctblpg() {
    void* phys_page = pmm_falloc(1);
    if (!phys_page) return NULL;

    page_table_t* virt_page = (page_table_t*)(hhdm_offset + (u64)phys_page);
    memset(virt_page, 0, 4096);

    return virt_page;
}

u64 vmm_get_phys(page_table_t* pml4v, u64 virt) {
    if (!pml4v) return 0;

    u64 pml4e = pml4v[PML4_IDX(virt)];
    if (!(pml4e & PAGE_PRESENT)) return 0;
    page_table_t* pdpt_virt = (page_table_t*)(hhdm_offset + (pml4e & ~0xFFFULL));

    u64 pdpte = pdpt_virt[PDPT_IDX(virt)];
    if (!(pdpte & PAGE_PRESENT)) return 0;
    page_table_t* pd_virt = (page_table_t*)(hhdm_offset + (pdpte & ~0xFFFULL));

    u64 pde = pd_virt[PD_IDX(virt)];
    if (!(pde & PAGE_PRESENT)) return 0;

    if (pde & PAGE_HUGE) {
        u64 phys_frame = pde & ~0x1FFFFFULL;
        u64 page_offset = virt & 0x1FFFFFULL;
        return phys_frame | page_offset;
    }

    page_table_t* pt_virt = (page_table_t*)(hhdm_offset + (pde & ~0xFFFULL));

    u64 pte = pt_virt[PT_IDX(virt)];
    if (!(pte & PAGE_PRESENT)) return 0;

    u64 phys_frame = pte & ~0xFFFULL;
    u64 page_offset = virt & 0xFFFULL;
    return phys_frame | page_offset;
}

void vmm_map_page(page_table_t* pml4v, u64 virt, u64 phys, u64 flg) {
    u64 pml4e = pml4v[PML4_IDX(virt)];
    page_table_t* pdpt_virt;
    if (!(pml4e & PAGE_PRESENT)) {
        pdpt_virt = alloctblpg();
        pml4v[PML4_IDX(virt)] = ((u64)pdpt_virt - hhdm_offset) | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
    } else {
        pdpt_virt = (page_table_t*)(hhdm_offset + (pml4e & ~0xFFFULL));
    }

    u64 pdpte = pdpt_virt[PDPT_IDX(virt)];
    page_table_t* pd_virt;
    if (!(pdpte & PAGE_PRESENT)) {
        pd_virt = alloctblpg();
        pdpt_virt[PDPT_IDX(virt)] = ((u64)pd_virt - hhdm_offset) | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
    } else {
        pd_virt = (page_table_t*)(hhdm_offset + (pdpte & ~0xFFFULL));
    }

    u64 pde = pd_virt[PD_IDX(virt)];
    page_table_t* pt_virt;
    if (!(pde & PAGE_PRESENT)) {
        pt_virt = alloctblpg();
        pd_virt[PD_IDX(virt)] = ((u64)pt_virt - hhdm_offset) | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
    } else {
        pt_virt = (page_table_t*)(hhdm_offset + (pde & ~0xFFFULL));
    }

    pt_virt[PT_IDX(virt)] = (phys & ~0xFFFULL) | flg | PAGE_PRESENT;
    asm volatile("invlpg (%0)" :: "r"(virt) : "memory");
}

void vmm_map_huge_page(page_table_t* pml4v, u64 virt, u64 phys, u64 flg) {
    u64 pml4e = pml4v[PML4_IDX(virt)];
    page_table_t* pdpt_virt;
    if (!(pml4e & PAGE_PRESENT)) {
        pdpt_virt = alloctblpg();
        pml4v[PML4_IDX(virt)] = ((u64)pdpt_virt - hhdm_offset) | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
    } else {
        pdpt_virt = (page_table_t*)(hhdm_offset + (pml4e & ~0xFFFULL));
    }

    u64 pdpte = pdpt_virt[PDPT_IDX(virt)];
    page_table_t* pd_virt;
    if (!(pdpte & PAGE_PRESENT)) {
        pd_virt = alloctblpg();
        pdpt_virt[PDPT_IDX(virt)] = ((u64)pd_virt - hhdm_offset) | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
    } else {
        pd_virt = (page_table_t*)(hhdm_offset + (pdpte & ~0xFFFULL));
    }

    pd_virt[PD_IDX(virt)] = (phys & ~0x1FFFFFULL) | flg | PAGE_PRESENT | PAGE_HUGE;
}

void vmm_init() {    
    pml4 = alloctblpg();

    struct pmm_state* pmms = get_pmm_state();

    u64 tpmem = pmms->mem_high;
    for (u64 i = 0; i < tpmem; i += 0x200000) {
        vmm_map_huge_page(pml4, HHDM_START + i, i, PAGE_WRITE);
    }

    u64 kphys = kaddr_req.response->physical_base;
    u64 klen  = (u64)&_kernel_end - (u64)&_kernel_start;

    for (u64 i = 0; i < klen; i += 4096) {
        vmm_map_page(pml4, KERN_START + i, kphys + i, PAGE_WRITE);
    }

    u64 lpml4p;
    asm volatile("mov %%cr3, %0" : "=r"(lpml4p));
    page_table_t* lpml4v = (page_table_t*)(hhdm_offset + lpml4p);
    for (int i = 256; i < 512; i++) {
        if (pml4[i] == 0 && lpml4v[i] != 0) {
            pml4[i] = lpml4v[i];
        }
    }

    u64 pml4p = (u64)pml4 - hhdm_offset;
    asm volatile("mov %0, %%cr3" :: "r"(pml4p) : "memory");

    lhhdm_off = hhdm_request.response->offset;
    hhdm_offset = HHDM_START;

    vmmr_head = allocvmmr();
    if (vmmr_head) {
        vmmr_head->vaddr_base = HEAP_START;
        vmmr_head->page_count = HEAP_SIZE / 4096;
        vmmr_head->is_free = 1;
        vmmr_head->next = NULL;
    }
}

void* xlate_limptr(void* limine_vaddr) {
    if (!limine_vaddr) return NULL;
    u64 paddr = (u64)limine_vaddr - lhhdm_off;
    return (void*)(paddr + HHDM_START);
}

static u32 is_table_empty(page_table_t* table_virt) {
    for (int i = 0; i < 512; i++) {
        if (table_virt[i] != 0) {
            return 0;
        }
    }
    return 1;
}

void vmm_unmap_page(page_table_t* pml4v, u64 virt, u64 flags) {
    extern u64 hhdm_offset;

    u64 pml4idx = PML4_IDX(virt);
    u64 pml4e = pml4v[pml4idx];
    if (!(pml4e & PAGE_PRESENT)) return; 
    
    u64 pdptp = pml4e & ~0xFFFULL;
    page_table_t* pdptv = (page_table_t*)(hhdm_offset + pdptp);

    u64 pdptidx = PDPT_IDX(virt);
    u64 pdpte = pdptv[pdptidx];
    if (!(pdpte & PAGE_PRESENT)) return;

    u64 pdp = pdpte & ~0xFFFULL;
    page_table_t* pdv = (page_table_t*)(hhdm_offset + pdp);

    u64 pdidx = PD_IDX(virt);
    u64 pde = pdv[pdidx];
    if (!(pde & PAGE_PRESENT)) return;

    u64 ptp = pde & ~0xFFFULL;
    page_table_t* ptv = (page_table_t*)(hhdm_offset + ptp);

    u64 ptidx = PT_IDX(virt);
    u64 pframe = ptv[ptidx] & ~0xFFFULL;
    ptv[ptidx] = 0;

    asm volatile("invlpg (%0)" :: "r"(virt) : "memory");

    if ((flags & UNMAP_KEEPPHYS) != UNMAP_KEEPPHYS) {
        pmm_ffree((void*)pframe, 1);
    }

    if (is_table_empty(ptv)) {
        pmm_ffree((void*)ptp, 1);
        pdv[pdidx] = 0;
        if (is_table_empty(pdv)) {
            pmm_ffree((void*)pdp, 1);
            pdptv[pdptidx] = 0;
            if (is_table_empty(pdptv)) {
                pmm_ffree((void*)pdptp, 1);
                pml4v[pml4idx] = 0;
            }
        }
    }
}

void* vmm_map_pages(page_table_t* pml4v, u64 vst, u64 pst, size_t pgcnt, u64 flg) {
    u64 page_size = 4096;
    u64 x86flgs = flg & ~(MAP_ANYPHYS | MAP_CONT | MAP_ANYVIRT);

    if (flg & MAP_ANYVIRT) {
        vst = vmm_ffreer(pgcnt);
        if (!vst) return NULL; 
    }

    if (flg & MAP_ANYPHYS) {
        if (flg & MAP_CONT) {
            void* bphys = pmm_falloc(pgcnt);
            if (!bphys) return NULL;
            pst = (u64)bphys;
            for (size_t i = 0; i < pgcnt; i++) {
                vmm_map_page(pml4v, vst + (i * page_size), pst + (i * page_size), x86flgs);
            }
            return (void*)vst;
        } else {
            for (size_t i = 0; i < pgcnt; i++) {
                void* phys_page = pmm_falloc(1);
                if (!phys_page) return NULL; 
                vmm_map_page(pml4v, vst + (i * page_size), (u64)phys_page, x86flgs);
            }
            return (void*)vst;
        }
    }

    for (size_t i = 0; i < pgcnt; i++) {
        vmm_map_page(pml4v, vst + (i * page_size), pst + (i * page_size), x86flgs);
    }
    return (void*)vst;
}

void vmm_unmap_pages(page_table_t* pml4v, u64 vst, size_t pgcnt, u64 flags) {
    u64 page_size = 4096;
    u64 szrm = pgcnt * page_size;
    for (size_t i = 0; i < pgcnt; i++) {
        vmm_unmap_page(pml4v, vst + (i * page_size), flags);
    }
    if (vst < HEAP_START || vst >= HEAP_END) return; 

    vmm_region_t* curr = vmmr_head;
    while (curr) {
        if (!curr->is_free && vst >= curr->vaddr_base && 
            (vst + szrm) <= (curr->vaddr_base + (curr->page_count * page_size))) {
            
            u64 node_end = curr->vaddr_base + (curr->page_count * page_size);
            u64 free_end = vst + szrm;

            if (vst == curr->vaddr_base && free_end == node_end) {
                curr->is_free = 1;
            } else if (vst == curr->vaddr_base) {
                vmm_region_t* left = allocvmmr();
                if (left) {
                    left->vaddr_base = free_end;
                    left->page_count = curr->page_count - pgcnt;
                    left->is_free = 0;
                    left->next = curr->next;

                    curr->page_count = pgcnt;
                    curr->is_free = 1;
                    curr->next = left;
                }
            } else if (free_end == node_end) {
                vmm_region_t* left = allocvmmr();
                if (left) {
                    left->vaddr_base = vst;
                    left->page_count = pgcnt;
                    left->is_free = 1;
                    left->next = curr->next;

                    curr->page_count -= pgcnt;
                    curr->next = left;
                }
            } else {
                vmm_region_t* mid = allocvmmr();
                vmm_region_t* trail = allocvmmr();

                if (mid && trail) {
                    trail->vaddr_base = free_end;
                    trail->page_count = (node_end - free_end) / page_size;
                    trail->is_free = 0;
                    trail->next = curr->next;

                    mid->vaddr_base = vst;
                    mid->page_count = pgcnt;
                    mid->is_free = 1;
                    mid->next = trail;

                    curr->page_count = (vst - curr->vaddr_base) / page_size;
                    curr->next = mid;
                }
            }
            break; 
        }
        curr = curr->next;
    }

    curr = vmmr_head;
    while (curr && curr->next) {
        if (curr->is_free && curr->next->is_free) {
            vmm_region_t* next_node = curr->next;
            curr->page_count += next_node->page_count;
            curr->next = next_node->next;
            
            next_node->vaddr_base = 0;
            next_node->page_count = 0;
            next_node->is_free = 0;
            next_node->next = NULL;
        } else {
            curr = curr->next;
        }
    }
}

page_table_t* vmm_cpml4v() {
    u64 cr3_phys;
    asm volatile("mov %%cr3, %0" : "=r"(cr3_phys));
    cr3_phys &= ~0xFFFULL; 
    return (page_table_t*)(hhdm_offset + cr3_phys);
}

page_table_t* vmm_casp() {
    page_table_t* npml4 = alloctblpg();
    if (!npml4) return NULL;
    for (int i = 256; i < 512; i++) npml4[i] = pml4[i]; 
    return npml4;
}

void vmm_sasp(page_table_t* tpml4) {
    if (!tpml4) return;
    u64 ppml4 = (u64)tpml4 - hhdm_offset;
    asm volatile("mov %0, %%cr3" :: "r"(ppml4) : "memory");
}

void vmm_skasp() {
    if (!pml4) return;
    vmm_sasp(pml4);
}

void vmm_dasp(page_table_t* tpml4) {
    for (u64 pml4_idx = 0; pml4_idx < 256; pml4_idx++) {
        u64 pml4e = tpml4[pml4_idx];
        if (!(pml4e & PAGE_PRESENT)) continue;
        page_table_t* pdpt = (page_table_t*)(hhdm_offset + (pml4e & ~0xFFFULL));
        for (u64 pdpt_idx = 0; pdpt_idx < 512; pdpt_idx++) {
            u64 pdpte = pdpt[pdpt_idx];
            if (!(pdpte & PAGE_PRESENT)) continue;
            page_table_t* pd = (page_table_t*)(hhdm_offset + (pdpte & ~0xFFFULL));
            for (u64 pd_idx = 0; pd_idx < 512; pd_idx++) {
                u64 pde = pd[pd_idx];
                if (!(pde & PAGE_PRESENT)) continue;
                page_table_t* pt = (page_table_t*)(hhdm_offset + (pde & ~0xFFFULL));
                for (u64 pt_idx = 0; pt_idx < 512; pt_idx++) {
                    u64 pte = pt[pt_idx];
                    if (!(pte & PAGE_PRESENT)) continue;

                    u64 pframe = pte & ~0xFFFULL;
                    pmm_ffree((void*)pframe, 1);
                    pt[pt_idx] = 0;
                }
                u64 ptp = pde & ~0xFFFULL;
                pmm_ffree((void*)ptp, 1);
                pd[pd_idx] = 0;
            }
            u64 pdp = pdpte & ~0xFFFULL;
            pmm_ffree((void*)pdp, 1);
            pdpt[pdpt_idx] = 0;
        }

        u64 pdptp = pml4e & ~0xFFFULL;
        pmm_ffree((void*)pdptp, 1);
        tpml4[pml4_idx] = 0;
    }

    u64 ppml4 = (u64)tpml4 - hhdm_offset;
    pmm_ffree((void*)ppml4, 1);
}