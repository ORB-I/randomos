#pragma once
#include <core/std.h>

bool rdseed_available(void);
int rdseed_init(void);
int rdseed_read(u8* buf, usize len);
