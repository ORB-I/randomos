#pragma once
#include <core/std.h>

void init_kbd();

char getchar(void);
usize getstr(char* buf, usize ntoread);
void enable_kbd();
void disable_kbd();
s32 kbd_enabled();

char* readline(const char* prompt);