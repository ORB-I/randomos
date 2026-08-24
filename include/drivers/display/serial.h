#pragma once

#include <core/std.h>
#include <stdarg.h>

int serial_isempty();
void serial_putchar(char c);
void serial_puts(const char *str);
int serial_printf(const char* fmt, ...);
int serial_vprintf(const char* fmt, va_list lst);