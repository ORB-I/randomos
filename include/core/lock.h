#pragma once
#include <stdatomic.h>
#include <core/std.h>

typedef struct {
    atomic_int lock;
    u64 rflags;
} lock_t;

void lock_init(lock_t* lk);
void lock_acquire(lock_t* lk);
void lock_release(lock_t* lk);