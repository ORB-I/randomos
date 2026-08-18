#pragma once
#include <core/std.h>

u64 init_tsc();
u64 get_tscms();
void tsc_sleep(u64 ms);