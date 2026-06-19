#pragma once
#include <core/std.h>

void* pmm_falloc(size_t cnt);
void pmm_ffree(void* ptr, size_t cnt);
void pmm_init();

/*struct memstat {
    u32 total_pages;
    u32 pagesz;
    u32 avail_pages;
};

s32 memstat(struct memstat* mst);*/