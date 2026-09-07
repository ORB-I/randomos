#pragma once
#include <core/std.h>

bool tpm_rng_available(void);
int tpm_rng_init(void);
int tpm_rng_read(u8* buf, usize len);
