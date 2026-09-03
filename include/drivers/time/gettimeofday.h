#pragma once
#include <core/std.h>

struct __packed millitime {
    u64 epoch;
    u64 ms;
};

void init_gettimeofday();
void getmtimeofday(struct millitime* mt);
u64 gettimeofday();