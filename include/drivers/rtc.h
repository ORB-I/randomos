#pragma once

#include <core/std.h>

void init_rtc();
void enable_rtc();
void disable_rtc();
void reset_rtc();
u32 rtc_getticks();
void rtc_sleep(s32 secs);
s32 rtc_enabled();