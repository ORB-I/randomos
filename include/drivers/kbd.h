#pragma once
#include <core/std.h>

void init_kbd();

void noecho(int on);
char getchar(void);
usize getstr(char* buf, usize ntoread);
void enable_kbd();
void disable_kbd();
s32 kbd_enabled();

void enqueue_key(char c);
char* readline(const char* prompt);

u8 kbd_get_raw(void);