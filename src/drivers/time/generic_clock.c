#include <drivers/time/clock.h>
#include <drivers/time/hpet.h>
#include <drivers/time/tsc.h>
#include <core/printf.h>
#include <core/errno.h>

u64 (*getms)(void) = NULL;

void sleepms(u64 ms) {
    u64 st = getms();
    while ((getms() - st) < ms);
}

int init_clock(int type) {
    if (type == CLOCK_HPET) {
        if (hpet_init(&getms) < 0) {
            return -1;
        } else {
            return 0;
        }
    } else if (type == CLOCK_TSC) {
        return init_tsc(&getms);
    } else {
        return -ENOEXIST;
    }
}
