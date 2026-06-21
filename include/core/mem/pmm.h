#pragma once
#include <core/std.h>

struct pmm_state {
    u64 mem_high;
    u64 mem_low;

    u32 nframes;
    u32 mxframe;
    u32 framesz;

    u8* mmap;
    u64 mmap_sz;
};

void* pmm_falloc(size_t cnt);
void pmm_ffree(void* ptr, size_t cnt);
void pmm_init();
struct pmm_state* get_pmm_state();

/*struct memstat {
    u32 total_pages;
    u32 pagesz;
    u32 avail_pages;
};

s32 memstat(struct memstat* mst);*/