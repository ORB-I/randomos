// this isnt really a proper driver
// its just a wrapper around 2 other
// drivers for a syscall

#include <drivers/rtc.h>
#include <drivers/tsc.h>
#include <drivers/gettimeofday.h>

u64 _gettimeofday_initime;
u64 _gettimeofday_initsc;
void init_gettimeofday() {
    _gettimeofday_initime = rtc_gettime();
    _gettimeofday_initsc  = get_tscms();
}

void getmtimeofday(struct millitime* mt) {
    u64 mss = get_tscms() - _gettimeofday_initsc;
    mt->epoch = _gettimeofday_initime + (mss / 1000);
    mt->ms = mss % 1000;
}

u64 gettimeofday() {
    return _gettimeofday_initime + ((get_tscms() - _gettimeofday_initsc) / 1000);
}