// this isnt really a proper driver
// its just a wrapper around 2 other
// drivers for a syscall

#include <drivers/time/rtc.h>
#include <drivers/time/clock.h>
#include <drivers/time/gettimeofday.h>

u64 _gettimeofday_initime;
u64 _gettimeofday_initsc;
void init_gettimeofday() {
    _gettimeofday_initime = rtc_gettime();
    _gettimeofday_initsc  = getms();
}

void getmtimeofday(struct millitime* mt) {
    u64 mss = getms() - _gettimeofday_initsc;
    mt->epoch = _gettimeofday_initime + (mss / 1000);
    mt->ms = mss % 1000;
}

u64 gettimeofday() {
    return _gettimeofday_initime + ((getms() - _gettimeofday_initsc) / 1000);
}
