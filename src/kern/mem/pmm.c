#include <core/std.h>
#include <lib/string.h>
#include <core/limreqs.h>
#include <core/panic.h>
#include <core/mem/pmm.h>

extern u64 _kernel_end;
extern u64 _kernel_start;
u64 hhdm_offset = 0;

static struct pmm_state state;

struct pmm_state* get_pmm_state() {
    return &state;
}

u32 pmm_try_resv(u32 stidx, u32 cnt) {
    if (stidx + cnt > state.nframes) return 0;
    for (u32 i = 0; i < cnt; i++) {
        u32 fidx = stidx + i;
        if (state.mmap[fidx / 8] & (1 << (fidx % 8))) {
            return 0;
        }
    }

    for (u32 i = 0; i < cnt; i++) {
        u32 fidx = stidx + i;
        state.mmap[fidx / 8] |= (1 << (fidx % 8));
    }

    return 1;
}

void* pmm_falloc(size_t cnt) {
    for (u32 i = 0; i < state.mmap_sz; i++) {
        if (state.mmap[i] == 0xFF) continue;
        for (u8 bit = 0; bit < 8; bit++) {
            u32 fidx = (i * 8) + bit;
            if (pmm_try_resv(fidx, cnt)) {
                return (void*)(state.mem_low + (fidx * state.framesz));
            }
        }
    }
    return NULL;
}

void pmm_ffree(void* ptr, size_t cnt) {
    u64 addr = (u64)ptr;
    if (addr < state.mem_low || addr >= state.mem_high || cnt == 0) return;
    u32 stidx = (addr - state.mem_low) / state.framesz;
    for (u32 i = 0; i < cnt; i++) {
        u32 fidx = stidx + i;
        if (fidx >= state.nframes) break;
        state.mmap[fidx / 8] &= ~(1 << (fidx % 8));
    }
}

void pmm_init() {
    hhdm_offset = hhdm_request.response->offset;

    memset(&state, 0, sizeof(state));
    state.framesz = 4096;

    for (u64 i = 0; i < mmap_req.response->entry_count; i++) {
        struct limine_memmap_entry* entry = mmap_req.response->entries[i];
        u64 eend = entry->base + entry->length;
        if (eend > state.mem_high) state.mem_high = eend;
    }

    if (state.mem_high == 0) panic("Couldn't find usable memory");

    state.nframes = state.mem_high / state.framesz;
    state.mmap_sz = state.nframes / 8;
    if (state.nframes % 8 != 0) state.mmap_sz++;

    u64 bmapaddr = 0;
    for (u64 i = 0; i < mmap_req.response->entry_count; i++) {
        struct limine_memmap_entry* entry = mmap_req.response->entries[i];
        if (entry->type == LIMINE_MEMMAP_USABLE && entry->length >= state.mmap_sz) {
            bmapaddr = entry->base;
            break;
        }
    }

    if (bmapaddr == 0) panic("Failed to find a large enough usable block for PMM bitmap!");

    state.mmap = (u8*)(hhdm_offset + bmapaddr);
    for (u32 i = 0; i < state.mmap_sz; i++) {
        state.mmap[i] = 0xFF;
    }
    state.mem_low = 0;

    for (u64 i = 0; i < mmap_req.response->entry_count; i++) {
        struct limine_memmap_entry* entry = mmap_req.response->entries[i];
        if (entry->type == LIMINE_MEMMAP_USABLE) {
            u64 sidx = (entry->base + state.framesz - 1) / state.framesz;
            u64 eidx = (entry->base + entry->length) / state.framesz;
            
            if (eidx > sidx) {
                pmm_ffree((void*)(sidx * state.framesz), eidx - sidx);
            }
        }
    }

    u64 bmapsidx = bmapaddr / state.framesz;
    u64 bmapfcnt = (state.mmap_sz + state.framesz - 1) / state.framesz;
    pmm_try_resv(bmapsidx, bmapfcnt);
}

/*s32 memstat(struct memstat* mst) {
    mst->pagesz = pmm_pagesz;
    mst->total_pages = pmm_npages;
    mst->avail_pages = 0;
    for (u32 i = 0; i < pmm_npages; i++) {
        for (s32 bit = 0; bit < 8; bit++) {
            if ((pmm_bitmap[i] & (1 << bit)) == 0) mst->avail_pages++;
        }
    }
    return 0;
}*/