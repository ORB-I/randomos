#pragma once
#include <sys/types.h>

#define MMAP_ADDRANY 0

void* mmap(void* addr, u64 npages);
int munmap(void* addr, u64 npages);

void* malloc(usize size);
void* realloc(void* ptr, usize size);
void* calloc(usize count, usize size);
void  free(void* ptr);