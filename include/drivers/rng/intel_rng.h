#pragma once
#include <core/std.h>

bool intel_rng_available(void);
int intel_rng_init(void);
int intel_rng_read(u8* buf, usize len);
