#pragma once
#include <core/std.h>

typedef int(*random_byte_cb)(void);

u64 random64(void);
int random_bytes(u8* buf, usize sz);
int rng_init();