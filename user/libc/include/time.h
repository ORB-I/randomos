#pragma once
#include <sys/types.h>

struct millitime {
    u64 epoch;
    u64 ms;
} __attribute__((packed));

typedef struct {
    u64 epoch; // unix epoch
    u64 ms;    // 0-999
    u64 sec;   // 0-59
    u64 min;   // 0-59
    u64 hrs;   // 0-24
    u64 day;   // start = 0
    u64 mon;   // 0 = jan
    u64 yr;    // >= 1970
    u8  leap;  // is a leap year
} ctime_t;

#define CLOCK_MONOMS 1
#define CLOCK_MONO   2
#define CLOCK_UNIX   3

u64 getclock(int clock);
u64 gettime();
void gettime_ms(struct millitime* timebuf);
void get_ctime(ctime_t* ct);