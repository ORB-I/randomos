#pragma once
#include <core/std.h>

bool jitter_rng_available(void);
int jitter_rng_init(void);
int jitter_rng_read(u8* buf, usize len);
