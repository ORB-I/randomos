#include <time.h>
#include <sys/syscall.h>
#include <str.h>
#include <mem.h>

u64 getclock(int clock) {
    if (clock == CLOCK_MONOMS) {
        return __syscall0(SYS_GETTIMEMONOMS);
    } else if (clock == CLOCK_MONO) {
        return __syscall0(SYS_GETTIMEMONO);
    } else if (clock == CLOCK_UNIX) {
        return __syscall0(SYS_GETTIMEOFDAY);
    } else {
        return 0;
    }
}

u64 gettime() {
    return getclock(CLOCK_UNIX);
}

void gettime_ms(struct millitime* timebuf) {
    __syscall1(SYS_GETMTIMEOFDAY, (u64)timebuf);
}

u8 _time_isleapyear(u64 year) {
    return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) ? 1 : 0;
}

void get_ctime(ctime_t* ct) {
    memset(ct, 0, sizeof(*ct));

    struct millitime tb;
    gettime_ms(&tb);

    ct->epoch = tb.epoch;
    ct->ms    = tb.ms;

    u64 dmos[2][12] = {
        {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}, // Normal year
        {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}  // Leap year
    };

    ct->day = tb.epoch / 86400;
    u64 rem = tb.epoch % 86400;

    ct->sec = rem % 60;
    ct->min = (rem % 3600) / 60;
    ct->hrs = rem / 3600;

    ct->yr = 1970;
    while (1) {
        ct->leap = _time_isleapyear(ct->yr);
        u64 dty = ct->leap ? 366 : 365;

        if (ct->day >= dty) {
            ct->day -= dty;
            ct->yr++;
        } else {
            break;
        }
    }

    while (ct->day >= dmos[ct->leap][ct->mon]) {
        ct->day = dmos[ct->leap][ct->mon];
        ct->mon++;
    }
}