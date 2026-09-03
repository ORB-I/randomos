#pragma once
#include <core/std.h>

int kprint_init();
int kprint_initdev();
int kprint(const char* fmt, ...) __printf(1, 2);
int kvprint(const char* fmt, va_list ap);