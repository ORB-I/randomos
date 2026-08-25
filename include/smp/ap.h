#pragma once
#include <core/std.h>

int ap_run(void(*fn)(void*), void* arg);
void ap_pause(u64 apicid);
void ap_continue(u64 apicid);
void ap_stop(u64 apicid);