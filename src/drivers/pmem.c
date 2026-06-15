#include <core/std.h>
#include <core/multiboot.h>
#include <core/panic.h>
#include <core/mem.h>

#include <drivers/vga.h>

#define KHEAP_MAX 0x00400000

extern u32 _kernel_end;
u32 pmm_start = 0;
u32 pmm_end = 0;

u32 pmm_pagesz = 4096;
u32 pmm_npages = 0;

u8* pmm_bitmap = 0;
u32 pmm_bmapsz = 0;

u32 pgbytes(u32 nbytes) {
    return (nbytes + (pmm_pagesz - 1)) / pmm_pagesz;
}

void pmem_init(multiboot_info_t* mbinfo) {
    printf("MEM: Initializing memory manager\n");
    pmm_start = _kernel_end;
    pmm_start = (pmm_start + 0xFFF) & ~0xFFF;

    multiboot_memory_map_t* mmap = (multiboot_memory_map_t*)mbinfo->mmap_addr;
    u32 mmap_end = mbinfo->mmap_addr + mbinfo->mmap_length;

    u32 highaddr = 0;

    while ((u32)mmap < mmap_end) {
        if (mmap->type == MULTIBOOT_MEMORY_AVAILABLE) {
            uint64_t eend = mmap->addr + mmap->len;
            if (eend <= 0xFFFFFFFF && (u32)eend > highaddr) highaddr = (u32)eend;
        }

        mmap = (multiboot_memory_map_t*)((u32)mmap + mmap->size + sizeof(mmap->size));
    }

    if (highaddr == 0) panic("Couldn't find usable memory");
    if (highaddr > KHEAP_MAX) {
        pmm_end = KHEAP_MAX;
    } else {
        pmm_end = highaddr;
    }

    pmm_bitmap = (u8*)_kernel_end;
    u32 mxpag = (pmm_end - (u32)pmm_bitmap) / pmm_pagesz;
    pmm_bmapsz = mxpag / 8;
    if (mxpag % 8 != 0) pmm_bmapsz++;

    pmm_start = (u32)pmm_bitmap + pmm_bmapsz;
    pmm_start = (pmm_start + 0xFFF) & ~0xFFF;

    pmm_npages = (pmm_end - pmm_start) / pmm_pagesz;
    
    pmm_bmapsz = pmm_npages / 8;
    if (pmm_npages % 8 != 0) pmm_bmapsz++;

    for (u32 i = 0; i < pmm_bmapsz; i++) {
        pmm_bitmap[i] = 0x00; 
    }
}

s32 pmem_try_resv(u32 stidx, u32 cnt) {
    if (stidx + cnt > pmm_npages) return 0;
    for (u32 i = 0; i < cnt; i++) {
        u32 pidx = stidx + i;
        if (pmm_bitmap[pidx / 8] & (1 << (pidx % 8))) {
            return 0;
        }
    }

    for (u32 i = 0; i < cnt; i++) {
        u32 pidx = stidx + i;
        pmm_bitmap[pidx / 8] |= (1 << (pidx % 8));
    }

    return 1;
}

void* pmem_alloc(u32 bytes) {
    u32 pgneeded = pgbytes(bytes);
    for (u32 i = 0; i < pmm_bmapsz; i++) {
        if (pmm_bitmap[i] == 0xFF) continue;

        for (s32 bit = 0; bit < 8; bit++) {
            u32 pidx = (i * 8) + bit;
            if (pmem_try_resv(pidx, pgneeded)) {
                return (void*)(pmm_start + (pidx * pmm_pagesz));
            }
        }
    }
    return NULL;
}

void pmem_free(void* ptr, u32 bytes) {
    u32 pgtofree = pgbytes(bytes);
    u32 addr = (u32)ptr;

    if (addr < pmm_start || addr >= pmm_end || pgtofree == 0) return;
    u32 stidx = (addr - pmm_start) / pmm_pagesz;

    for (u32 i = 0; i < pgtofree; i++) {
        u32 pgidx = stidx + i;
        if (pgidx >= pmm_npages) break;
        pmm_bitmap[pgidx / 8] &= ~(1 << (pgidx % 8));
    }
}

void* pmem_realloc(void* ptr, u32 oldsz, u32 newsz) {
    if (!ptr) return pmem_alloc(newsz);
    if (newsz == 0) {
        pmem_free(ptr, oldsz);
        return NULL;
    }

    u32 opgc = pgbytes(oldsz);
    u32 npgc = pgbytes(newsz);

    if (opgc == npgc) return ptr;
    u32 addr = (u32)ptr;
    u32 stidx = (addr - pmm_start) / pmm_pagesz;

    if (npgc < opgc) {
        u32 pgtofree = opgc - npgc;
        u32 fstidx = stidx + npgc;

        for (u32 i = 0; i < pgtofree; i++) {
            pmm_bitmap[(fstidx + i) / 8] &= ~(1 << ((fstidx + i) % 8));
        }

        return ptr;
    }

    u32 pgneeded = npgc - opgc;
    u32 chk_stidx = stidx + opgc;
    
    if (pmem_try_resv(chk_stidx, pgneeded)) {
        return ptr;
    }

    void* new_ptr = pmem_alloc(newsz);
    if (!new_ptr) return NULL;
    for (u32 i = 0; i < oldsz; i++) {
        ((u8*)new_ptr)[i] = ((u8*)ptr)[i];
    }

    pmem_free(ptr, oldsz);
    return new_ptr;
}

s32 memstat(struct memstat* mst) {
    mst->pagesz = pmm_pagesz;
    mst->total_pages = pmm_npages;
    mst->avail_pages = 0;
    for (u32 i = 0; i < pmm_npages; i++) {
        for (s32 bit = 0; bit < 8; bit++) {
            if ((pmm_bitmap[i] & (1 << bit)) == 0) mst->avail_pages++;
        }
    }
    return 0;
}