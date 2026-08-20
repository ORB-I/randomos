#pragma once
#include <core/std.h>

#define CLOCK_TSC  1
#define CLOCK_HPET 2

extern u64 (*getms)(void);
void sleepms(u64 ms);
int init_clock(int type);
