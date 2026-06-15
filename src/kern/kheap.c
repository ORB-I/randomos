#include <core/mem.h>
#include <core/std.h>

typedef struct kmalloc_hdr {
    u32 size;
    u8 is_free;
    struct kmalloc_hdr* next;
} kmalloc_hdr_t;

kmalloc_hdr_t* heap_start = NULL;
u32 pgallocated = 0;

void* kmalloc(u32 size) {
    size = (size + 4) & ~3;
    kmalloc_hdr_t* curr = heap_start;

    while (curr) {
        if (curr->is_free && curr->size >= size) {
            if (curr->size >= size + sizeof(kmalloc_hdr_t) + 4) {
                kmalloc_hdr_t* nblock = (kmalloc_hdr_t*)((u32)curr + sizeof(kmalloc_hdr_t) + size);
                nblock->size = curr->size - size - sizeof(kmalloc_hdr_t);
                nblock->is_free = 1;
                nblock->next = curr->next;

                curr->size = size;
                curr->next = nblock;
            }
            curr->is_free = 0;
            return (void*)((u32)curr + sizeof(kmalloc_hdr_t));
        }
        curr = curr->next;
    }

    u32 pgneeded = (size + sizeof(kmalloc_hdr_t) + 4095) / 4096;
    kmalloc_hdr_t* new_pgblk = (kmalloc_hdr_t*)pmem_alloc(pgneeded * 4096);
    if (!new_pgblk) return NULL;

    new_pgblk->size = (pgneeded * 4096) - sizeof(kmalloc_hdr_t);
    new_pgblk->is_free = 1;
    new_pgblk->next = NULL;

    if (!heap_start) {
        heap_start = new_pgblk;
    } else {
        curr = heap_start;
        while (curr->next) curr = curr->next;
        curr->next = new_pgblk;
    }

    return kmalloc(size);
}

void* krealloc(void* ptr, u32 newsz) {
    if (!ptr) return kmalloc(newsz);
    if (newsz == 0) {
        kfree(ptr);
        return NULL;
    }
    newsz = (newsz + 3) & ~3;

    kmalloc_hdr_t* curr = (kmalloc_hdr_t*)((u32)ptr - sizeof(kmalloc_hdr_t));
    if (curr->size == newsz) return ptr;

    if (newsz < curr->size) {
        if (curr->size - newsz >= sizeof(kmalloc_hdr_t) + 4) {
            kmalloc_hdr_t* nblock = (kmalloc_hdr_t*)((u32)curr + sizeof(kmalloc_hdr_t) + newsz);
            nblock->size = curr->size - newsz - sizeof(kmalloc_hdr_t);
            nblock->is_free = 1;
            nblock->next = curr->next;

            curr->size = newsz;
            curr->next = nblock;
        }
        return ptr;
    }

    if (curr->next && curr->next->is_free) {
        u32 cbspace = curr->size + sizeof(kmalloc_hdr_t) + curr->next->size;
        
        if (cbspace >= newsz) {
            u32 leftover = cbspace - newsz;
            if (leftover >= sizeof(kmalloc_hdr_t) + 4) {
                kmalloc_hdr_t* nnext = (kmalloc_hdr_t*)((u32)curr + sizeof(kmalloc_hdr_t) + newsz);
                nnext->size = leftover - sizeof(kmalloc_hdr_t);
                nnext->is_free = 1;
                nnext->next = curr->next->next;

                curr->size = newsz;
                curr->next = nnext;
            } else {
                curr->size = cbspace;
                curr->next = curr->next->next;
            }
            return ptr;
        }
    }

    void* new_ptr = kmalloc(newsz);
    for (u32 i = 0; i < curr->size; i++) {
        ((u8*)new_ptr)[i] = ((u8*)ptr)[i];
    }

    kfree(ptr);
    return new_ptr;
}

void kfree(void* ptr) {
    if (!ptr) return;

    kmalloc_hdr_t* hdr = (kmalloc_hdr_t*)((u32)ptr - sizeof(kmalloc_hdr_t));
    hdr->is_free = 1;

    kmalloc_hdr_t* curr = heap_start;
    while (curr && curr->next) {
        if (curr->is_free && curr->next->is_free) {
            curr->size += sizeof(kmalloc_hdr_t) + curr->next->size;
            curr->next = curr->next->next;
        } else {
            curr = curr->next;
        }
    }
}