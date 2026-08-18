#pragma once
#include <core/std.h>

struct millitime {
    u64 epoch;
    u64 ms;
} __attribute__((packed));

void init_gettimeofday();
void getmtimeofday(struct millitime* mt);
u64 gettimeofday();