#pragma once
#include <core/std.h>
#include <core/multiboot.h>

void pmem_init(multiboot_info_t* mbinfo);

void* pmem_alloc(u32 bytes);
void pmem_free(void* ptr, u32 bytes);
void* pmem_realloc(void* ptr, u32 oldsz, u32 newsz);

void* kmalloc(u32 size);
void* krealloc(void* ptr, u32 newsz);
void kfree(void* ptr);

struct memstat {
    u32 total_pages;
    u32 pagesz;
    u32 avail_pages;
};

s32 memstat(struct memstat* mst);