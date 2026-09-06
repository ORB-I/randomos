#pragma once
#include <core/std.h>

bool via_rng_available(void);
int via_rng_init(void);
int via_rng_read(u8* buf, usize len);
