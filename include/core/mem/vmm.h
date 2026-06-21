#pragma once
#include <core/std.h>

#define PAGE_PRESENT  (1ULL << 0)
#define PAGE_WRITE    (1ULL << 1)
#define PAGE_USER     (1ULL << 2)
#define PAGE_HUGE     (1ULL << 7)

#define MAP_ANYPHYS   (1ULL << 60)
#define MAP_CONT      (1ULL << 61)
#define MAP_ANYVIRT   (1ULL << 62)

#define PML4_IDX(addr) (((addr) >> 39) & 0x1FF)
#define PDPT_IDX(addr) (((addr) >> 30) & 0x1FF)
#define PD_IDX(addr)   (((addr) >> 21) & 0x1FF)
#define PT_IDX(addr)   (((addr) >> 12) & 0x1FF)

#define USER_START 0x0000000000000000
#define USER_END   0xFFFF800000000000
#define HHDM_START 0xFFFF800000000000
#define HHDM_END   0xFFFF880000000000
#define HHDM_SIZE  (HHDM_END - HHDM_START)
#define MMIO_START 0xFFFF880000000000
#define MMIO_END   0xFFFF900000000000
#define MMIO_SIZE  (MMIO_END - MMIO_START)
#define HEAP_START 0xFFFF900000000000
#define HEAP_END   0xFFFFB00000000000
#define HEAP_SIZE  (HEAP_END - HEAP_START)
#define KERN_START 0xFFFFFFFF80000000
#define KERN_END   0xFFFFFFFFC0000000
#define KERN_SIZE  (KERN_START - KERN_END)

typedef u64 page_table_t;

void vmm_init();
void vmm_map_page(page_table_t* pml4v, u64 virt, u64 phys, u64 flg);
void* vmm_map_pages(page_table_t* pml4v, u64 vst, u64 pst, size_t pgcnt, u64 flg);
void vmm_unmap_page(page_table_t* pml4v, u64 virt);
void vmm_unmap_pages(page_table_t* pml4v, u64 vst, size_t pgcnt);

page_table_t* vmm_cpml4v();
page_table_t* vmm_casp();
void vmm_sasp(page_table_t* tpml4);
void vmm_skasp();
void vmm_dasp(page_table_t* tpml4);

void* xlate_limptr(void* limine_vaddr);