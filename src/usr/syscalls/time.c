#include "ssc.h"
#include "../ensurance.h"
#include <scheduler/process.h>
#include <scheduler/scheduler.h>
#include <drivers/time/clock.h>
#include <drivers/time/gettimeofday.h>
#include <core/asmh.h>

DEFSYSCALL(sys_sleep) {
    if (args->a0 == 0) {
        preempt_pending = 1;
    } else {
        proctbl[current_pid].wake_ms = (getms ? getms() : 0) + args->a0;
        proctbl[current_pid].is_blocked = 1;
        preempt_pending = 1;
    }

    return 0;
}

DEFSYSCALL(sys_gettimeofday) {
    (void)args;
    return gettimeofday();
}

DEFSYSCALL(sys_getmtimeofday) {
    if (!ensure_pointer((void*)args->a0, sizeof(struct millitime), 1)) return -EINVAL;
    getmtimeofday((struct millitime*)args->a0);
    return 0;
}

DEFSYSCALL(sys_gettimemonoms) {
    (void)args;
    return getms();
}

DEFSYSCALL(sys_gettimemono) {
    (void)args;
    return rdtsc();
}