#pragma once

#include <sys/types.h>

u8 kbd_get_raw(void);
u8 kbd_get_raw_to(u64 timeout);
char getchar();