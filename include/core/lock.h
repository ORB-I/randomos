#pragma once

typedef struct {
    volatile int __lkst;
} spinlock_t;

void spl_init(spinlock_t* spl);
void spl_lock(spinlock_t* spl);
void spl_unlock(spinlock_t* spl);